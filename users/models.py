from django.contrib.auth.models import AbstractUser
from django.conf import settings
from django.db import models


class User(AbstractUser):
    """
    Custom user model with email-based authentication.
    In the doctor-centric architecture, users are doctors who manage patients
    and collect wearable data on their behalf.
    """

    ROLE_CHOICES = [
        ('doctor', 'Doctor'),
    ]

    email = models.EmailField(unique=True)
    role = models.CharField(
        max_length=20,
        choices=ROLE_CHOICES,
        default='doctor',
        help_text="User role in the system",
    )
    is_verified = models.BooleanField(
        default=False,
        help_text="Is account verified",
    )
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    USERNAME_FIELD = 'email'
    REQUIRED_FIELDS = ['username']

    class Meta:
        db_table = 'users'
        verbose_name = 'User'
        verbose_name_plural = 'Users'

    def __str__(self):
        return self.email


class Patient(models.Model):
    """
    Represents a patient in the study. Patients do not have login credentials;
    they are managed entirely by their doctors through the app.

    A patient can belong to multiple doctors (ManyToMany), supporting
    shared care scenarios.
    """

    GENDER_CHOICES = [
        ('male', 'Male'),
        ('female', 'Female'),
        ('other', 'Other'),
        ('prefer_not_to_say', 'Prefer not to say'),
    ]

    SUBSTANCE_TYPE_CHOICES = [
        ('alcohol', 'Alcohol'),
        ('opioids', 'Opioids'),
        ('cannabis', 'Cannabis'),
        ('stimulants', 'Stimulants'),
        ('benzodiazepines', 'Benzodiazepines'),
        ('tobacco', 'Tobacco'),
        ('polysubstance', 'Polysubstance'),
        ('other', 'Other'),
    ]

    doctors = models.ManyToManyField(
        settings.AUTH_USER_MODEL,
        related_name='patients',
        help_text="Doctors managing this patient",
    )
    name = models.CharField(
        max_length=255,
        help_text="Patient's full name",
    )
    date_of_birth = models.DateField(
        null=True, blank=True,
        help_text="Used for age-correlated ML analysis",
    )
    gender = models.CharField(
        max_length=20,
        choices=GENDER_CHOICES,
        blank=True,
        default='',
        help_text="Patient's gender",
    )
    device_id = models.CharField(
        max_length=100,
        blank=True,
        null=True,
        help_text="Unique identifier of the patient's wearable device",
    )
    substance_type = models.CharField(
        max_length=30,
        choices=SUBSTANCE_TYPE_CHOICES,
        blank=True,
        default='',
        help_text="Primary substance of concern for relapse monitoring",
    )
    diagnosis_notes = models.TextField(
        blank=True,
        default='',
        help_text="Clinical diagnosis details or relevant medical history",
    )
    notes = models.TextField(
        blank=True,
        default='',
        help_text="General notes about the patient",
    )
    is_active = models.BooleanField(
        default=True,
        help_text="Soft-delete flag; inactive patients are hidden from default lists",
    )
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        db_table = 'patients'
        ordering = ['-created_at']

    def __str__(self):
        return f"{self.name} (ID: {self.pk})"


class PasswordResetOTP(models.Model):
    """
    Stores a one-time password (OTP) code for password reset.
    OTPs expire after 10 minutes and can only be used once.
    """

    user = models.ForeignKey(
        settings.AUTH_USER_MODEL,
        on_delete=models.CASCADE,
        related_name='password_reset_otps',
    )
    code = models.CharField(
        max_length=6,
        help_text="6-digit OTP code",
    )
    is_used = models.BooleanField(default=False)
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        db_table = 'password_reset_otps'
        ordering = ['-created_at']

    def __str__(self):
        return f"OTP(user={self.user_id}, used={self.is_used})"
