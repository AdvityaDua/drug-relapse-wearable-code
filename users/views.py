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

from .models import PasswordResetOTP, Patient
from .serializers import (
    RegisterSerializer,
    LoginSerializer,
    UserProfileSerializer,
    PatientSerializer,
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

    Create a new doctor account and return JWT tokens.

    Request body:
        {
            "email": "doctor@example.com",
            "password": "securepass123",
            "password_confirm": "securepass123",
            "first_name": "Dr. Jane",      (optional)
            "last_name": "Smith"            (optional)
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
            "email": "doctor@example.com",
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
    GET  /api/auth/profile/ — Retrieve authenticated doctor's profile.
    PUT  /api/auth/profile/ — Update authenticated doctor's profile.

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


class PatientListCreateView(APIView):
    """
    GET  /api/auth/patients/
        List all patients managed by the authenticated doctor.
        Only active patients are shown by default; pass ?include_inactive=true
        to include soft-deleted patients.

    POST /api/auth/patients/
        Create a new patient and automatically assign the authenticated
        doctor to the patient's doctors list.

        Request body:
            {
                "name": "John Doe",
                "date_of_birth": "1990-05-15",       (optional)
                "gender": "male",                     (optional)
                "device_id": "WEARABLE-001",          (optional)
                "substance_type": "opioids",          (optional)
                "diagnosis_notes": "OUD since 2020",  (optional)
                "notes": "Weekly check-in"            (optional)
            }

    Requires: Authorization: Bearer <access_token>
    """

    permission_classes = [IsAuthenticated]

    def get(self, request):
        patients = Patient.objects.filter(doctors=request.user)

        include_inactive = request.query_params.get('include_inactive', '').lower()
        if include_inactive != 'true':
            patients = patients.filter(is_active=True)

        serializer = PatientSerializer(patients, many=True)
        return Response(serializer.data)

    def post(self, request):
        serializer = PatientSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        patient = serializer.save()

        # Automatically add the creating doctor to the patient's doctors
        patient.doctors.add(request.user)

        # Re-serialize to include the doctor in the response
        response_serializer = PatientSerializer(patient)
        return Response(response_serializer.data, status=status.HTTP_201_CREATED)


class PatientDetailView(APIView):
    """
    GET    /api/auth/patients/<id>/  — Retrieve a patient's details.
    PUT    /api/auth/patients/<id>/  — Update a patient's details.
    DELETE /api/auth/patients/<id>/  — Soft-delete a patient (set is_active=False).

    Only patients belonging to the authenticated doctor can be accessed.

    Requires: Authorization: Bearer <access_token>
    """

    permission_classes = [IsAuthenticated]

    def _get_patient(self, request, pk):
        """Helper to get a patient that belongs to the authenticated doctor."""
        try:
            return Patient.objects.filter(doctors=request.user).get(pk=pk)
        except Patient.DoesNotExist:
            return None

    def get(self, request, pk):
        patient = self._get_patient(request, pk)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )
        serializer = PatientSerializer(patient)
        return Response(serializer.data)

    def put(self, request, pk):
        patient = self._get_patient(request, pk)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )
        serializer = PatientSerializer(patient, data=request.data, partial=True)
        serializer.is_valid(raise_exception=True)
        serializer.save()
        return Response(serializer.data)

    def delete(self, request, pk):
        """Soft-delete: set is_active=False instead of removing the record."""
        patient = self._get_patient(request, pk)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )
        patient.is_active = False
        patient.save(update_fields=['is_active', 'updated_at'])
        return Response(
            {'message': 'Patient deactivated.'},
            status=status.HTTP_200_OK,
        )


class ForgotPasswordView(APIView):
    """
    POST /api/auth/forgot-password/

    Request a password reset OTP. A 6-digit code is generated and
    sent to the user's email.

    Request body:
        {"email": "doctor@example.com"}

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
        {"email": "doctor@example.com", "code": "123456"}

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
            "email": "doctor@example.com",
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
