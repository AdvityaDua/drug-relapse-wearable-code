from django.urls import path

from .views import SensorUploadView, SensorSyncView, SensorLatestView


urlpatterns = [
    path('upload/', SensorUploadView.as_view(), name='data-upload'),
    path('sync/', SensorSyncView.as_view(), name='data-sync'),
    path('latest/', SensorLatestView.as_view(), name='data-latest'),
]
