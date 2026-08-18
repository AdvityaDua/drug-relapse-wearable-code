from rest_framework import status
from rest_framework.permissions import AllowAny, IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework_simplejwt.views import TokenRefreshView as JWTTokenRefreshView

from .serializers import (
    RegisterSerializer,
    LoginSerializer,
    UserProfileSerializer,
    TokenPairSerializer,
)


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


# Re-export SimpleJWT's token refresh view
TokenRefreshView = JWTTokenRefreshView
