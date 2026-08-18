from rest_framework import serializers
from .models import SensorReading


class SensorReadingSerializer(serializers.ModelSerializer):
    """
    Serializes a single sensor reading.
    The 'user' field is set automatically from the authenticated request.
    """

    class Meta:
        model = SensorReading
        fields = '__all__'
        read_only_fields = ['id', 'user', 'synced_at']


class BulkSensorUploadSerializer(serializers.Serializer):
    """
    Accepts a list of sensor readings for batch upload from the mobile app.

    Expected payload:
    {
        "readings": [
            {"time": 1234567890, "gsr": 512, "hr": 72, ...},
            {"time": 1234567891, "gsr": 515, "hr": 73, ...},
        ]
    }
    """

    readings = serializers.ListField(
        child=serializers.DictField(),
        min_length=1,
        max_length=1000,
        help_text="Array of sensor reading objects (max 1000 per request)",
    )

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
        user = self.context['request'].user
        readings = validated_data['readings']

        # Build SensorReading objects
        reading_objects = []
        for reading_data in readings:
            reading_objects.append(
                SensorReading(user=user, **reading_data)
            )

        # Bulk create with ignore_conflicts for idempotent syncs
        created = SensorReading.objects.bulk_create(
            reading_objects,
            ignore_conflicts=True,
        )
        return created
