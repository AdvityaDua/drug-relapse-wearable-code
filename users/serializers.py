from django.contrib.auth import get_user_model
from django.contrib.auth.password_validation import validate_password
from rest_framework import serializers
from rest_framework_simplejwt.tokens import RefreshToken

from .models import Patient


User = get_user_model()


class RegisterSerializer(serializers.ModelSerializer):
    """
    Handles doctor registration.
    Accepts email + password + optional name fields.
    Returns the created user data (password excluded).
    """

    password = serializers.CharField(
        write_only=True,
        min_length=8,
        validators=[validate_password],
    )
    password_confirm = serializers.CharField(write_only=True, min_length=8)

    class Meta:
        model = User
        fields = [
            'email', 'password', 'password_confirm',
            'first_name', 'last_name',
        ]
        extra_kwargs = {
            'first_name': {'required': False},
            'last_name': {'required': False},
        }

    def validate(self, attrs):
        if attrs['password'] != attrs['password_confirm']:
            raise serializers.ValidationError(
                {"password_confirm": "Passwords do not match."}
            )
        return attrs

    def create(self, validated_data):
        validated_data.pop('password_confirm')
        password = validated_data.pop('password')

        # Auto-generate username from email prefix
        email = validated_data['email']
        base_username = email.split('@')[0]
        username = base_username
        counter = 1
        while User.objects.filter(username=username).exists():
            username = f"{base_username}{counter}"
            counter += 1
        validated_data['username'] = username

        user = User(**validated_data)
        user.set_password(password)
        user.save()
        return user


class LoginSerializer(serializers.Serializer):
    """
    Takes email + password, validates credentials.
    """

    email = serializers.EmailField()
    password = serializers.CharField(write_only=True)

    def validate(self, attrs):
        email = attrs['email']
        password = attrs['password']

        try:
            user = User.objects.get(email=email)
        except User.DoesNotExist:
            raise serializers.ValidationError(
                {"email": "No account found with this email."}
            )

        if not user.check_password(password):
            raise serializers.ValidationError(
                {"password": "Incorrect password."}
            )

        if not user.is_active:
            raise serializers.ValidationError(
                {"email": "This account is deactivated."}
            )

        attrs['user'] = user
        return attrs


class UserProfileSerializer(serializers.ModelSerializer):
    """
    Read/update doctor profile information.
    """

    class Meta:
        model = User
        fields = [
            'id', 'email', 'username', 'first_name', 'last_name',
            'role', 'is_verified', 'created_at', 'updated_at',
        ]
        read_only_fields = ['id', 'email', 'username', 'role', 'is_verified', 'created_at', 'updated_at']


class PatientSerializer(serializers.ModelSerializer):
    """
    Full CRUD serializer for Patient records.
    The authenticated doctor is automatically added to the patient's doctors.
    """

    doctors = serializers.PrimaryKeyRelatedField(
        many=True,
        read_only=True,
    )

    class Meta:
        model = Patient
        fields = [
            'id', 'doctors', 'name', 'date_of_birth', 'gender',
            'device_id', 'substance_type', 'diagnosis_notes', 'notes',
            'is_active', 'created_at', 'updated_at',
        ]
        read_only_fields = ['id', 'doctors', 'created_at', 'updated_at']


class TokenPairSerializer(serializers.Serializer):
    """
    Utility serializer to represent JWT token pair in responses.
    """

    access = serializers.CharField(read_only=True)
    refresh = serializers.CharField(read_only=True)

    @staticmethod
    def get_tokens_for_user(user):
        refresh = RefreshToken.for_user(user)
        return {
            'access': str(refresh.access_token),
            'refresh': str(refresh),
        }


class ForgotPasswordSerializer(serializers.Serializer):
    """
    Accepts email to initiate password reset.
    """

    email = serializers.EmailField()

    def validate_email(self, value):
        try:
            User.objects.get(email=value)
        except User.DoesNotExist:
            raise serializers.ValidationError("No account found with this email.")
        return value


class VerifyOTPSerializer(serializers.Serializer):
    """
    Verifies that the OTP code is valid for the given email.
    """

    email = serializers.EmailField()
    code = serializers.CharField(max_length=6, min_length=6)


class ResetPasswordSerializer(serializers.Serializer):
    """
    Resets the password using a verified OTP.
    """

    email = serializers.EmailField()
    code = serializers.CharField(max_length=6, min_length=6)
    new_password = serializers.CharField(
        write_only=True,
        min_length=8,
        validators=[validate_password],
    )
    new_password_confirm = serializers.CharField(write_only=True, min_length=8)

    def validate(self, attrs):
        if attrs['new_password'] != attrs['new_password_confirm']:
            raise serializers.ValidationError(
                {"new_password_confirm": "Passwords do not match."}
            )
        return attrs
