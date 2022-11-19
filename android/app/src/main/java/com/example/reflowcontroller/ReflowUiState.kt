package com.example.reflowcontroller

/**
 * Data class that represents the game UI state
 */
data class ReflowUiState(
    val connected: Boolean = false,
    val temperature: Int = 0,
    val isRunning: Boolean = false
)
