package com.example.reflowcontroller

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.companion.AssociationRequest
import android.companion.BluetoothDeviceFilter
import android.companion.CompanionDeviceManager
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.ActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.core.app.ActivityCompat
import androidx.core.content.ContentProviderCompat.requireContext
import com.example.reflowcontroller.ui.theme.ReflowControllerTheme
import com.example.reflowcontroller.ReflowController
import java.util.concurrent.Executor
import java.util.regex.Pattern

/*
@RequiresApi(Build.VERSION_CODES.O)
class MainActivity : ComponentActivity() {

    private val REQUEST_ENABLE_BT: Int = 1

    val btAdapter: BluetoothAdapter by lazy {
        val java = BluetoothManager::class.java
        getSystemService(java)!!.adapter }

    @RequiresApi(Build.VERSION_CODES.O)
    override fun onCreate(savedInstanceState: Bundle?) {
        // setup bluetooth.
        // see https://developer.android.com/guide/topics/connectivity/bluetooth/setup

        if (btAdapter== null) {
            // Device doesn't support Bluetooth
            // TODO do something
            return
        }

        // request to turn on bluetooth
        if (btAdapter?.isEnabled == false) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            if (ActivityCompat.checkSelfPermission(
                    this,
                    Manifest.permission.BLUETOOTH_CONNECT
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                // TODO: Consider calling
                //    ActivityCompat#requestPermissions
                // here to request the missing permissions, and then overriding
                //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                //                                          int[] grantResults)
                // to handle the case where the user grants the permission. See the documentation
                // for ActivityCompat#requestPermissions for more details.
                return
            }
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
        }

        super.onCreate(savedInstanceState)

        setContent {
            ReflowControllerTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colors.background
                ) {
                    ReflowController(btAdapter)
                }
            }
        }

        // To skip filters based on names and supported feature flags (UUIDs),
        // omit calls to setNamePattern() and addServiceUuid()
        // respectively, as shown in the following  Bluetooth example.
        val deviceFilter: BluetoothDeviceFilter = BluetoothDeviceFilter.Builder()
            .setNamePattern(Pattern.compile("HC-06"))
            .build()

        // The argument provided in setSingleDevice() determines whether a single
        // device name or a list of them appears.
        val pairingRequest: AssociationRequest = AssociationRequest.Builder()
            .addDeviceFilter(deviceFilter)
            .setSingleDevice(true)
            .build()

        val deviceManager =
            requireContext().getSystemService(Context.COMPANION_DEVICE_SERVICE)

        val executor: Executor =  Executor { it.run() }

        deviceManager.associate(pairingRequest,
            executor,
            object : CompanionDeviceManager.Callback() {
                // Called when a device is found. Launch the IntentSender so the user
                // can select the device they want to pair with.
                override fun onAssociationPending(intentSender: IntentSender) {
                    intentSender?.let {
                        startIntentSenderForResult(it, SELECT_DEVICE_REQUEST_CODE, null, 0, 0, 0)
                    }
                }

                override fun onAssociationCreated(associationInfo: AssociationInfo) {
                    // The association is created.
                }

                override fun onFailure(errorMessage: CharSequence?) {
                    // Handle the failure.
                }
            })
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            REQUEST_ENABLE_BT ->
                if (resultCode == Activity.RESULT_OK) {
                    // bluetooth enabled
                } else {
                    // not enabled
                }
            SELECT_DEVICE_REQUEST_CODE -> when(resultCode) {
                Activity.RESULT_OK -> {
                    // The user chose to pair the app with a Bluetooth device.
                    val deviceToPair: BluetoothDevice? =
                        data?.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE)
                    deviceToPair?.let { device ->
                        device.createBond()
                        // Maintain continuous interaction with a paired device.
                    }
                }
            }
            else -> super.onActivityResult(requestCode, resultCode, data)
        }
    }
}
*/









private const val SELECT_DEVICE_REQUEST_CODE = 0

class MainActivity : AppCompatActivity() {

    private val deviceManager: CompanionDeviceManager by lazy {
        getSystemService(Context.COMPANION_DEVICE_SERVICE) as CompanionDeviceManager
    }
    val mBluetoothAdapter: BluetoothAdapter by lazy {
        val java = BluetoothManager::class.java
        getSystemService(java)!!.adapter
    }
    val executor: Executor = Executor { it.run() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // To skip filters based on names and supported feature flags (UUIDs),
        // omit calls to setNamePattern() and addServiceUuid()
        // respectively, as shown in the following  Bluetooth example.
        val deviceFilter: BluetoothDeviceFilter = BluetoothDeviceFilter.Builder()
            .setNamePattern(Pattern.compile("My device"))
            .addServiceUuid(ParcelUuid(UUID(0x123abcL, -1L)), null)
            .build()

        // The argument provided in setSingleDevice() determines whether a single
        // device name or a list of them appears.
        val pairingRequest: AssociationRequest = AssociationRequest.Builder()
            .addDeviceFilter(deviceFilter)
            .setSingleDevice(true)
            .build()

        // When the app tries to pair with a Bluetooth device, show the
        // corresponding dialog box to the user.
        deviceManager.associate(pairingRequest,
            executor,
            object : CompanionDeviceManager.Callback() {
                // Called when a device is found. Launch the IntentSender so the user
                // can select the device they want to pair with.
                override fun onAssociationPending(intentSender: IntentSender) {
                    intentSender?.let {
                        startIntentSenderForResult(it, SELECT_DEVICE_REQUEST_CODE, null, 0, 0, 0)
                    }
                }

                override fun onAssociationCreated(associationInfo: AssociationInfo) {
                    // AssociationInfo object is created and get association id and the
                    // macAddress.
                    var associationId: int = associationInfo.id
                    var macAddress: MacAddress = associationInfo.deviceMacAddress
                }

                override fun onFailure(errorMessage: CharSequence?) {
                    // Handle the failure.
                }
                )

                override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
                    when (requestCode) {
                        SELECT_DEVICE_REQUEST_CODE -> when (resultCode) {
                            Activity.RESULT_OK -> {
                                // The user chose to pair the app with a Bluetooth device.
                                val deviceToPair: BluetoothDevice? =
                                    data?.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE)
                                deviceToPair?.let { device ->
                                    device.createBond()
                                    // Maintain continuous interaction with a paired device.
                                }
                            }
                        }
                        else -> super.onActivityResult(requestCode, resultCode, data)
                    }
                }
            }

                    open class AppCompatActivity<> {

            }
