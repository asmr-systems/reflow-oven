package com.example.reflowcontroller

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.annotation.RequiresApi
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.app.ActivityCompat.startActivityForResult
import androidx.core.content.ContextCompat.getSystemService
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import com.example.reflowcontroller.ReflowUiState

/**
 * ViewModel containing the app data and methods to process the data
 */
class ReflowViewModel(btAdapter: BluetoothAdapter): ViewModel() {

    // Reflow UI state
    private val _uiState = MutableStateFlow(ReflowUiState())
    val uiState: StateFlow<ReflowUiState> = _uiState.asStateFlow()

    // Bluetooth Adapter
    lateinit var bt: BluetoothAdapter

    init {
        bt = btAdapter

        connect()
    }

    fun connect() {

    }

    fun startReflow() {

    }

    fun stopReflow() {

    }
}