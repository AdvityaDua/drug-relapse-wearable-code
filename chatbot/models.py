from django.conf import settings
from django.db import models


class Conversation(models.Model):
    """
    A chat conversation between a user and the AI assistant.
    Users can have multiple conversations; each one maintains its own history.
    """

    user = models.ForeignKey(
        settings.AUTH_USER_MODEL,
        on_delete=models.CASCADE,
        related_name='conversations',
    )
    title = models.CharField(
        max_length=255,
        blank=True,
        default='',
        help_text="Auto-generated or user-set title for the conversation",
    )
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        db_table = 'chatbot_conversations'
        ordering = ['-updated_at']

    def __str__(self):
        return f"Conversation(user={self.user_id}, title={self.title!r})"


class Message(models.Model):
    """
    A single message in a conversation.
    Role is one of: 'system', 'user', 'assistant'.
    """

    ROLE_CHOICES = [
        ('system', 'System'),
        ('user', 'User'),
        ('assistant', 'Assistant'),
    ]

    conversation = models.ForeignKey(
        Conversation,
        on_delete=models.CASCADE,
        related_name='messages',
    )
    role = models.CharField(
        max_length=10,
        choices=ROLE_CHOICES,
    )
    content = models.TextField(
        help_text="The message text content",
    )
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        db_table = 'chatbot_messages'
        ordering = ['created_at']

    def __str__(self):
        return f"Message(role={self.role}, conv={self.conversation_id})"
