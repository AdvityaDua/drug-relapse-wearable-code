from django.urls import path

from .views import (
    RegisterView,
    LoginView,
    ProfileView,
    TokenRefreshView,
    ForgotPasswordView,
    VerifyOTPView,
    ResetPasswordView,
)


urlpatterns = [
    path('register/', RegisterView.as_view(), name='auth-register'),
    path('login/', LoginView.as_view(), name='auth-login'),
    path('profile/', ProfileView.as_view(), name='auth-profile'),
    path('token/refresh/', TokenRefreshView.as_view(), name='token-refresh'),
    path('forgot-password/', ForgotPasswordView.as_view(), name='auth-forgot-password'),
    path('verify-otp/', VerifyOTPView.as_view(), name='auth-verify-otp'),
    path('reset-password/', ResetPasswordView.as_view(), name='auth-reset-password'),
]
