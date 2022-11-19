package com.example.reflowcontroller

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import com.example.reflowcontroller.ReflowUiState

/**
 * ViewModel containing the app data and methods to process the data
 */
class ReflowViewModel : ViewModel() {

    // Game UI state
    private val _uiState = MutableStateFlow(ReflowUiState())
    val uiState: StateFlow<ReflowUiState> = _uiState.asStateFlow()

    init {
        connect()
    }

    fun connect() {

    }

    fun startReflow() {

    }

    fun stopReflow() {

    }
}