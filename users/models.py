from django.contrib.auth.models import AbstractUser
from django.conf import settings
from django.db import models


class User(AbstractUser):
    """
    Custom user model with email-based authentication.
    Extends Django's AbstractUser to add wearable-specific profile fields.
    """

    email = models.EmailField(unique=True)
    device_id = models.CharField(
        max_length=100,
        blank=True,
        null=True,
        help_text="Unique identifier of the user's wearable device",
    )
    date_of_birth = models.DateField(
        blank=True,
        null=True,
        help_text="Used for age-correlated ML analysis",
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
