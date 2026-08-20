from rest_framework import serializers
from .models import DataCollectionDay, SensorReading


class DataCollectionDaySerializer(serializers.ModelSerializer):
    """
    Serializes a DataCollectionDay.
    Includes a computed 'reading_count' for listing views.
    The 'user' field is set automatically from the authenticated request.
    """

    reading_count = serializers.IntegerField(read_only=True, required=False)

    class Meta:
        model = DataCollectionDay
        fields = ['id', 'date', 'reading_count', 'created_at', 'updated_at']
        read_only_fields = ['id', 'created_at', 'updated_at']


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
        """Ensure the collection day exists and belongs to the authenticated user."""
        user = self.context['request'].user
        try:
            day = DataCollectionDay.objects.get(pk=value, user=user)
        except DataCollectionDay.DoesNotExist:
            raise serializers.ValidationError(
                "Collection day not found or does not belong to you."
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
