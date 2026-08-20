from django.conf import settings
from django.db import models


class DataCollectionDay(models.Model):
    """
    Represents a single calendar day of data collection for a user.
    Created by the mobile app before syncing readings for that day.
    Unique constraint on (user, date) ensures one entry per user per day.
    """

    user = models.ForeignKey(
        settings.AUTH_USER_MODEL,
        on_delete=models.CASCADE,
        related_name='collection_days',
    )
    date = models.DateField(
        help_text="The calendar date of data collection (YYYY-MM-DD)",
    )
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        db_table = 'data_collection_days'
        constraints = [
            models.UniqueConstraint(
                fields=['user', 'date'],
                name='unique_user_collection_day',
            ),
        ]
        indexes = [
            models.Index(fields=['user', 'date'], name='idx_user_date'),
        ]
        ordering = ['-date']

    def __str__(self):
        return f"CollectionDay(user={self.user_id}, date={self.date})"


class SensorReading(models.Model):
    """
    Single timestamped sensor reading from the wearable device.
    Contains all 35 sensor fields from the data collection pipeline.
    Linked to a DataCollectionDay; the user is derived via collection_day.user.
    Composite unique constraint on (collection_day, time) ensures idempotent syncs.
    """

    collection_day = models.ForeignKey(
        DataCollectionDay,
        on_delete=models.CASCADE,
        related_name='readings',
        help_text="The data collection day this reading belongs to",
    )

    # ── Timestamp ──────────────────────────────────────────────────────
    time = models.PositiveBigIntegerField(
        help_text="Unix Epoch timestamp in seconds",
    )

    # ── Galvanic Skin Response (TLA2022 ADC) ──────────────────────────
    gsr = models.SmallIntegerField(
        blank=True, null=True,
        help_text="GSR raw analog value",
    )

    # ── Body Temperature (MAX30205) ───────────────────────────────────
    bodyTemp = models.FloatField(
        blank=True, null=True,
        help_text="Clinical body temperature in Celsius",
    )

    # ── Heart Rate & SpO2 (MAX30102) ──────────────────────────────────
    hr = models.IntegerField(
        blank=True, null=True,
        help_text="Heart Rate in bpm",
    )
    validHR = models.SmallIntegerField(
        blank=True, null=True,
        help_text="Boolean flag: 1=reliable HR, 0=unreliable",
    )
    spo2 = models.IntegerField(
        blank=True, null=True,
        help_text="Blood Oxygen Saturation percentage",
    )
    validSPO2 = models.SmallIntegerField(
        blank=True, null=True,
        help_text="Boolean flag: 1=reliable SpO2, 0=unreliable",
    )

    # ── BNO055 Euler Angles ───────────────────────────────────────────
    bno055_euler_heading = models.FloatField(
        blank=True, null=True,
        help_text="Euler angle heading (yaw) in degrees",
    )
    bno055_euler_roll = models.FloatField(
        blank=True, null=True,
        help_text="Euler angle roll in degrees",
    )
    bno055_euler_pitch = models.FloatField(
        blank=True, null=True,
        help_text="Euler angle pitch in degrees",
    )

    # ── BNO055 Quaternion ─────────────────────────────────────────────
    bno055_quat_w = models.FloatField(
        blank=True, null=True,
        help_text="Quaternion W component",
    )
    bno055_quat_x = models.FloatField(
        blank=True, null=True,
        help_text="Quaternion X component",
    )
    bno055_quat_y = models.FloatField(
        blank=True, null=True,
        help_text="Quaternion Y component",
    )
    bno055_quat_z = models.FloatField(
        blank=True, null=True,
        help_text="Quaternion Z component",
    )

    # ── BNO055 Linear Acceleration (m/s²) ─────────────────────────────
    bno055_linear_x = models.FloatField(
        blank=True, null=True,
        help_text="Linear acceleration X excluding gravity",
    )
    bno055_linear_y = models.FloatField(
        blank=True, null=True,
        help_text="Linear acceleration Y excluding gravity",
    )
    bno055_linear_z = models.FloatField(
        blank=True, null=True,
        help_text="Linear acceleration Z excluding gravity",
    )

    # ── BNO055 Gravity Vector (m/s²) ──────────────────────────────────
    bno055_gravity_x = models.FloatField(
        blank=True, null=True,
        help_text="Gravity vector X",
    )
    bno055_gravity_y = models.FloatField(
        blank=True, null=True,
        help_text="Gravity vector Y",
    )
    bno055_gravity_z = models.FloatField(
        blank=True, null=True,
        help_text="Gravity vector Z",
    )

    # ── BNO055 Raw Acceleration (m/s²) ────────────────────────────────
    bno055_accel_x = models.FloatField(
        blank=True, null=True,
        help_text="Raw acceleration X including gravity",
    )
    bno055_accel_y = models.FloatField(
        blank=True, null=True,
        help_text="Raw acceleration Y including gravity",
    )
    bno055_accel_z = models.FloatField(
        blank=True, null=True,
        help_text="Raw acceleration Z including gravity",
    )

    # ── BNO055 Gyroscope (degrees/second) ─────────────────────────────
    bno055_gyro_x = models.FloatField(
        blank=True, null=True,
        help_text="Angular velocity X",
    )
    bno055_gyro_y = models.FloatField(
        blank=True, null=True,
        help_text="Angular velocity Y",
    )
    bno055_gyro_z = models.FloatField(
        blank=True, null=True,
        help_text="Angular velocity Z",
    )

    # ── BNO055 Magnetometer (microteslas) ─────────────────────────────
    bno055_mag_x = models.FloatField(
        blank=True, null=True,
        help_text="Magnetic field X",
    )
    bno055_mag_y = models.FloatField(
        blank=True, null=True,
        help_text="Magnetic field Y",
    )
    bno055_mag_z = models.FloatField(
        blank=True, null=True,
        help_text="Magnetic field Z",
    )

    # ── BNO055 Temperature & Calibration ──────────────────────────────
    bno055_temp = models.SmallIntegerField(
        blank=True, null=True,
        help_text="IMU chip temperature in Celsius",
    )
    bno055_calib_sys = models.SmallIntegerField(
        blank=True, null=True,
        help_text="System calibration status (0-3)",
    )
    bno055_calib_gyro = models.SmallIntegerField(
        blank=True, null=True,
        help_text="Gyroscope calibration status (0-3)",
    )
    bno055_calib_accel = models.SmallIntegerField(
        blank=True, null=True,
        help_text="Accelerometer calibration status (0-3)",
    )
    bno055_calib_mag = models.SmallIntegerField(
        blank=True, null=True,
        help_text="Magnetometer calibration status (0-3)",
    )

    # ── Sync Metadata ─────────────────────────────────────────────────
    synced_at = models.DateTimeField(
        auto_now_add=True,
        help_text="Server-side timestamp when this reading was synced",
    )

    class Meta:
        db_table = 'sensor_readings'
        # Prevent duplicate readings for the same collection day at the same timestamp
        constraints = [
            models.UniqueConstraint(
                fields=['collection_day', 'time'],
                name='unique_reading_per_day_timestamp',
            ),
        ]
        indexes = [
            models.Index(fields=['collection_day', 'time'], name='idx_collection_day_time'),
            models.Index(fields=['time'], name='idx_time'),
        ]
        ordering = ['-time']

    def __str__(self):
        return f"Reading(day={self.collection_day_id}, time={self.time})"
