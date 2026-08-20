from django.urls import path

from .views import (
    DataCollectionDayView,
    DataCollectionDayDetailView,
    SensorUploadView,
    SensorSyncView,
    SensorLatestView,
)


urlpatterns = [
    path('days/', DataCollectionDayView.as_view(), name='collection-days'),
    path('days/<int:pk>/', DataCollectionDayDetailView.as_view(), name='collection-day-detail'),
    path('upload/', SensorUploadView.as_view(), name='data-upload'),
    path('sync/', SensorSyncView.as_view(), name='data-sync'),
    path('latest/', SensorLatestView.as_view(), name='data-latest'),
]
