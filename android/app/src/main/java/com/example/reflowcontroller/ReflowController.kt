package com.example.reflowcontroller

import androidx.compose.foundation.layout.*
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.reflowcontroller.ReflowViewModel
import com.example.reflowcontroller.ui.theme.ReflowControllerTheme

@Composable
fun ReflowController(
    modifier: Modifier = Modifier,
    reflowViewModel: ReflowViewModel = viewModel()
) {
    val reflowUiState: ReflowUiState by reflowViewModel.uiState.collectAsState()

    Column(
        modifier = modifier
            .padding(16.dp)
    ) {
        ConnectionStatus(
            connected = reflowUiState.connected
        )
        ReflowLayout(
            connected = reflowUiState.connected,
            temperature = reflowUiState.temperature,
            isRunning = reflowUiState.isRunning,
            reflowViewModel = reflowViewModel,
        )
    }
}

@Composable
fun ReflowLayout(
    connected: Boolean,
    temperature: Int,
    isRunning: Boolean,
    reflowViewModel: ReflowViewModel,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 30.dp),
    ) {
        Text(text = "temperature: $temperature")
        Row(
            modifier = modifier
                .fillMaxWidth()
                .padding(top = 16.dp),
            horizontalArrangement = Arrangement.SpaceAround,
        ) {
            Button(
                modifier = modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(start = 2.dp),
                onClick = {
                    if (connected) {
                        if (isRunning) {
                            reflowViewModel.stopReflow()
                        } else {
                            reflowViewModel.startReflow()
                        }
                    } else {
                        reflowViewModel.connect()
                    }
                }
            ) {
                Text(if(!connected) {"connect"} else {if (isRunning) {"stop"} else {"start"}})
            }
        }
    }
}

@Composable
fun ConnectionStatus(
    connected: Boolean,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
    ) {
        Text(
            text = if (connected) {
                "connected to reflow oven :)"
            } else {
                "not connected to reflow oven :("
            },
            fontSize = 14.sp,
            color = if (connected){Color.Blue} else {Color.Red},
        )
    }
}

@Preview(showBackground = true)
@Composable
fun ReflowControllerPreview() {
    ReflowControllerTheme {
        ReflowController()
    }
}