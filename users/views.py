import random
import logging
from datetime import timedelta

from django.contrib.auth import get_user_model
from django.core.mail import send_mail
from django.conf import settings
from django.utils import timezone
from rest_framework import status
from rest_framework.permissions import AllowAny, IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework_simplejwt.views import TokenRefreshView as JWTTokenRefreshView

from .models import PasswordResetOTP
from .serializers import (
    RegisterSerializer,
    LoginSerializer,
    UserProfileSerializer,
    TokenPairSerializer,
    ForgotPasswordSerializer,
    VerifyOTPSerializer,
    ResetPasswordSerializer,
)

User = get_user_model()
logger = logging.getLogger(__name__)

# OTP expires after 10 minutes
OTP_EXPIRY_MINUTES = 10


class RegisterView(APIView):
    """
    POST /api/auth/register/

    Create a new user account and return JWT tokens.

    Request body:
        {
            "email": "user@example.com",
            "password": "securepass123",
            "password_confirm": "securepass123",
            "first_name": "John",          (optional)
            "last_name": "Doe",            (optional)
            "device_id": "WEARABLE-001",   (optional)
            "date_of_birth": "1995-06-15"  (optional)
        }

    Response (201):
        {
            "user": { ... },
            "tokens": { "access": "...", "refresh": "..." }
        }
    """

    permission_classes = [AllowAny]

    def post(self, request):
        serializer = RegisterSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        user = serializer.save()

        tokens = TokenPairSerializer.get_tokens_for_user(user)
        profile = UserProfileSerializer(user).data

        return Response(
            {
                'user': profile,
                'tokens': tokens,
            },
            status=status.HTTP_201_CREATED,
        )


class LoginView(APIView):
    """
    POST /api/auth/login/

    Authenticate with email + password and receive JWT tokens.

    Request body:
        {
            "email": "user@example.com",
            "password": "securepass123"
        }

    Response (200):
        {
            "user": { ... },
            "tokens": { "access": "...", "refresh": "..." }
        }
    """

    permission_classes = [AllowAny]

    def post(self, request):
        serializer = LoginSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        user = serializer.validated_data['user']

        tokens = TokenPairSerializer.get_tokens_for_user(user)
        profile = UserProfileSerializer(user).data

        return Response(
            {
                'user': profile,
                'tokens': tokens,
            },
            status=status.HTTP_200_OK,
        )


class ProfileView(APIView):
    """
    GET  /api/auth/profile/ — Retrieve authenticated user's profile.
    PUT  /api/auth/profile/ — Update authenticated user's profile.

    Requires: Authorization: Bearer <access_token>
    """

    permission_classes = [IsAuthenticated]

    def get(self, request):
        serializer = UserProfileSerializer(request.user)
        return Response(serializer.data)

    def put(self, request):
        serializer = UserProfileSerializer(
            request.user,
            data=request.data,
            partial=True,
        )
        serializer.is_valid(raise_exception=True)
        serializer.save()
        return Response(serializer.data)


class ForgotPasswordView(APIView):
    """
    POST /api/auth/forgot-password/

    Request a password reset OTP. A 6-digit code is generated and
    sent to the user's email.

    Request body:
        {"email": "user@example.com"}

    Response (200):
        {"message": "OTP sent to your email."}
    """

    permission_classes = [AllowAny]

    def post(self, request):
        serializer = ForgotPasswordSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        email = serializer.validated_data['email']
        user = User.objects.get(email=email)

        # Invalidate any previous unused OTPs for this user
        PasswordResetOTP.objects.filter(user=user, is_used=False).update(is_used=True)

        # Generate a 6-digit OTP
        code = f"{random.randint(0, 999999):06d}"

        PasswordResetOTP.objects.create(user=user, code=code)

        # Send OTP via email
        try:
            send_mail(
                subject='Password Reset OTP',
                message=(
                    f'Your password reset code is: {code}\n\n'
                    f'This code expires in {OTP_EXPIRY_MINUTES} minutes.\n'
                    f'If you did not request this, please ignore this email.'
                ),
                from_email=settings.DEFAULT_FROM_EMAIL if hasattr(settings, 'DEFAULT_FROM_EMAIL') else 'noreply@example.com',
                recipient_list=[email],
                fail_silently=False,
            )
        except Exception as e:
            logger.error(f"Failed to send OTP email: {e}")
            # Still return success to not leak info, but log the error
            # In development, the console email backend will print the OTP

        return Response(
            {'message': 'OTP sent to your email.'},
            status=status.HTTP_200_OK,
        )


class VerifyOTPView(APIView):
    """
    POST /api/auth/verify-otp/

    Verify that an OTP code is valid before allowing password reset.
    This is an optional step — the app can call this to show a
    "code verified" UI before asking for the new password.

    Request body:
        {"email": "user@example.com", "code": "123456"}

    Response (200):
        {"message": "OTP verified successfully."}

    Response (400):
        {"message": "Invalid or expired OTP."}
    """

    permission_classes = [AllowAny]

    def post(self, request):
        serializer = VerifyOTPSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        email = serializer.validated_data['email']
        code = serializer.validated_data['code']

        expiry_time = timezone.now() - timedelta(minutes=OTP_EXPIRY_MINUTES)

        otp = PasswordResetOTP.objects.filter(
            user__email=email,
            code=code,
            is_used=False,
            created_at__gte=expiry_time,
        ).first()

        if otp is None:
            return Response(
                {'message': 'Invalid or expired OTP.'},
                status=status.HTTP_400_BAD_REQUEST,
            )

        return Response(
            {'message': 'OTP verified successfully.'},
            status=status.HTTP_200_OK,
        )


class ResetPasswordView(APIView):
    """
    POST /api/auth/reset-password/

    Reset the password using a valid OTP code.

    Request body:
        {
            "email": "user@example.com",
            "code": "123456",
            "new_password": "newSecurePass123!",
            "new_password_confirm": "newSecurePass123!"
        }

    Response (200):
        {"message": "Password reset successfully."}

    Response (400):
        {"message": "Invalid or expired OTP."}
    """

    permission_classes = [AllowAny]

    def post(self, request):
        serializer = ResetPasswordSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        email = serializer.validated_data['email']
        code = serializer.validated_data['code']
        new_password = serializer.validated_data['new_password']

        expiry_time = timezone.now() - timedelta(minutes=OTP_EXPIRY_MINUTES)

        otp = PasswordResetOTP.objects.filter(
            user__email=email,
            code=code,
            is_used=False,
            created_at__gte=expiry_time,
        ).first()

        if otp is None:
            return Response(
                {'message': 'Invalid or expired OTP.'},
                status=status.HTTP_400_BAD_REQUEST,
            )

        # Mark OTP as used
        otp.is_used = True
        otp.save(update_fields=['is_used'])

        # Reset the password
        user = otp.user
        user.set_password(new_password)
        user.save(update_fields=['password'])

        return Response(
            {'message': 'Password reset successfully.'},
            status=status.HTTP_200_OK,
        )


# Re-export SimpleJWT's token refresh view
TokenRefreshView = JWTTokenRefreshView
