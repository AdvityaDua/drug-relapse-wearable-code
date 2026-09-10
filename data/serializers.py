from rest_framework import serializers

from users.models import Patient
from .models import DataCollectionDay, SensorReading


class DataCollectionDaySerializer(serializers.ModelSerializer):
    """
    Serializes a DataCollectionDay.
    Includes a computed 'reading_count' for listing views.
    Includes a computed 'day' field indicating the nth day since patient creation.
    The 'patient' field is set from the URL path parameter.
    """

    reading_count = serializers.IntegerField(read_only=True, required=False)
    patient = serializers.PrimaryKeyRelatedField(read_only=True)
    day = serializers.SerializerMethodField()
    device_id = serializers.CharField(required=False, allow_blank=True, allow_null=True)

    class Meta:
        model = DataCollectionDay
        fields = ['id', 'patient', 'date', 'day', 'device_id', 'reading_count', 'created_at', 'updated_at']
        read_only_fields = ['id', 'patient', 'day', 'created_at', 'updated_at']

    def get_day(self, obj):
        if obj.patient and obj.patient.created_at and obj.date:
            delta = obj.date - obj.patient.created_at.date()
            return delta.days + 1
        return None


class SensorReadingSerializer(serializers.ModelSerializer):
    """
    Serializes a single sensor reading.
    The 'collection_day' is set from context or request data.
    """

    class Meta:
        model = SensorReading
        fields = '__all__'
        read_only_fields = ['id', 'collection_day', 'synced_at']


class BulkSensorUploadSerializer(serializers.Serializer):
    """
    Accepts a list of sensor readings for batch upload from the mobile app.
    Readings are linked to a specific DataCollectionDay.

    The doctor must own the patient associated with the collection day.

    Expected payload:
    {
        "collection_day": 1,
        "readings": [
            {"time": 1234567890, "gsr": 512, "hr": 72, ...},
            {"time": 1234567891, "gsr": 515, "hr": 73, ...},
        ]
    }
    """

    collection_day = serializers.IntegerField(
        help_text="ID of the DataCollectionDay to link readings to",
    )
    readings = serializers.ListField(
        child=serializers.DictField(),
        min_length=1,
        max_length=1000,
        help_text="Array of sensor reading objects (max 1000 per request)",
    )

    def validate_collection_day(self, value):
        """Ensure the collection day exists and its patient belongs to the authenticated doctor."""
        doctor = self.context['request'].user
        try:
            day = DataCollectionDay.objects.get(
                pk=value,
                patient__doctors=doctor,
            )
        except DataCollectionDay.DoesNotExist:
            raise serializers.ValidationError(
                "Collection day not found or its patient does not belong to you."
            )
        return day

    def validate_readings(self, readings):
        """Validate each reading has a required 'time' field."""
        errors = []
        for i, reading in enumerate(readings):
            if 'time' not in reading:
                errors.append(f"Reading at index {i} is missing required 'time' field.")
        if errors:
            raise serializers.ValidationError(errors)
        return readings

    def create(self, validated_data):
        collection_day = validated_data['collection_day']
        readings = validated_data['readings']

        # Build SensorReading objects linked to the collection day
        reading_objects = [
            SensorReading(collection_day=collection_day, **reading_data)
            for reading_data in readings
        ]

        # Bulk create with ignore_conflicts for idempotent syncs
        created = SensorReading.objects.bulk_create(
            reading_objects,
            ignore_conflicts=True,
        )
        return created
