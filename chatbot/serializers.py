from rest_framework import serializers
from .models import Conversation, Message


class MessageSerializer(serializers.ModelSerializer):
    """Serializes a single chat message."""

    class Meta:
        model = Message
        fields = ['id', 'role', 'content', 'created_at']
        read_only_fields = ['id', 'role', 'created_at']


class ConversationSerializer(serializers.ModelSerializer):
    """Serializes a conversation with message count metadata."""

    message_count = serializers.IntegerField(read_only=True, required=False)

    class Meta:
        model = Conversation
        fields = ['id', 'title', 'message_count', 'created_at', 'updated_at']
        read_only_fields = ['id', 'created_at', 'updated_at']


class ConversationDetailSerializer(serializers.ModelSerializer):
    """Serializes a conversation with its full message history."""

    messages = MessageSerializer(many=True, read_only=True)

    class Meta:
        model = Conversation
        fields = ['id', 'title', 'messages', 'created_at', 'updated_at']
        read_only_fields = ['id', 'created_at', 'updated_at']


class ChatRequestSerializer(serializers.Serializer):
    """
    Validates an incoming chat request.

    Expected payload:
        {"message": "How is my heart rate looking today?"}

    Optionally include conversation_id to continue an existing conversation:
        {"message": "Tell me more", "conversation_id": 5}
    """

    message = serializers.CharField(
        max_length=4000,
        help_text="The user's message to send to the chatbot",
    )
    conversation_id = serializers.IntegerField(
        required=False,
        allow_null=True,
        help_text="ID of an existing conversation to continue. "
                  "Omit or null to start a new conversation.",
    )
