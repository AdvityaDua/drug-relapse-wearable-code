from django.urls import path

from .views import RegisterView, LoginView, ProfileView, TokenRefreshView


urlpatterns = [
    path('register/', RegisterView.as_view(), name='auth-register'),
    path('login/', LoginView.as_view(), name='auth-login'),
    path('profile/', ProfileView.as_view(), name='auth-profile'),
    path('token/refresh/', TokenRefreshView.as_view(), name='token-refresh'),
]
