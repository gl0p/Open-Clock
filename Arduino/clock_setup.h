/*
 * This file is part of the open-clock project.
 * 
 * Copyright (C) 2024 gl0p
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CLOCK_SETUP_H
#define CLOCK_SETUP_H

const char* clock_setup_html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Device Setup Wizard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        /* General Styles */
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 0; }
        .container { max-width: 500px; margin: 50px auto; background: #fff; padding: 20px; border-radius: 8px; }
        h1 { text-align: center; }
        .step { display: none; }
        .step.active { display: block; }
        label { display: block; margin-top: 10px; }
        input[type="text"], input[type="password"], select { width: 100%; padding: 8px; margin-top: 5px; border: 1px solid #ccc; border-radius: 4px; }
        button { padding: 10px 20px; margin-top: 20px; border: none; border-radius: 4px; cursor: pointer; }
        .buttons { text-align: center; }
        button:disabled { background-color: #ccc; }
        button:not(:disabled) { background-color: #007BFF; color: white; }
        .back-button { background-color: #6c757d; color: white; margin-right: 10px; }
        .hidden { display: none; }
        .message { text-align: center; margin-top: 20px; }
        /* iOS redirect styles */
        #redirectLink { color: #007BFF; text-decoration: none; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Setup Wizard</h1>
        <!-- Step 1: Wi-Fi Setup -->
        <div class="step active" id="step1">
            <h2>Step 1: Connect to Wi-Fi</h2>
            <label for="ssid">Wi-Fi Name (SSID):</label>
            <input type="text" id="ssid" required>
            <label for="password">Wi-Fi Password:</label>
            <input type="password" id="password" required>
            <div class="buttons">
                <button id="next1" disabled>Next</button>
            </div>
        </div>
        <!-- Step 2: Time Zone Setup -->
        <div class="step" id="step2">
            <h2>Step 2: Set Time Zone</h2>
            <label for="timezone">Time Zone:</label>
            <select id="timezone">
                <!-- Time zone options -->
                <!-- Populate via JavaScript -->
            </select>
            <div class="buttons">
                <button class="back-button" id="back2">Back</button>
                <button id="next2" disabled>Next</button>
            </div>
        </div>
        <!-- Step 3: Device Name Setup -->
        <div class="step" id="step3">
            <h2>Step 3: Device Name</h2>
            <label for="deviceName">Enter a name for your device:</label>
            <input type="text" id="deviceName" required>
            <div class="buttons">
                <button class="back-button" id="back3">Back</button>
                <button id="next3" disabled>Next</button>
            </div>
        </div>
        <!-- Step 4: Redirect -->
        <div class="step" id="step4">
            <h2>Setup Complete!</h2>
            <p class="message">Redirecting you to your device...</p>
            <p class="message hidden" id="redirectMessage">
                If you are not redirected automatically, please connect to your home Wi-Fi and click <a href="#" id="redirectLink">here</a>.
                <br>
                Note: If the above link doesn't work, try accessing your device using the IP address: <strong id="ipAddress"> in your browser seasrch bar.</strong>
            </p>        
        </div>
    </div>
    <script>
        // JavaScript for handling the wizard steps
        let currentStep = 1;
        const totalSteps = 4;

        const ssidInput = document.getElementById('ssid');
        const passwordInput = document.getElementById('password');
        const timezoneSelect = document.getElementById('timezone');
        const dstCheckbox = document.getElementById('dst');
        const deviceNameInput = document.getElementById('deviceName');

        const next1Button = document.getElementById('next1');
        const next2Button = document.getElementById('next2');
        const next3Button = document.getElementById('next3');

        const back2Button = document.getElementById('back2');
        const back3Button = document.getElementById('back3');

        const redirectMessage = document.getElementById('redirectMessage');
        const redirectLink = document.getElementById('redirectLink');

        // Time zones with POSIX tzCode
        const timezones = [
            // UTC−12:00
            { name: 'UTC−12:00', tzCode: 'UTC+12', location: 'Baker Island' },

            // UTC−11:00
            { name: 'UTC−11:00', tzCode: 'UTC+11', location: 'American Samoa' },

            // UTC−10:00 (Hawaii Standard Time, no DST)
            { name: 'Hawaii Standard Time', tzCode: 'HST10', location: 'Hawaii' },

            // UTC−09:00 (Alaska Standard Time)
            { name: 'Alaska Standard Time', tzCode: 'AKST9AKDT,M3.2.0,M11.1.0', location: 'Alaska' },

            // UTC−08:00 (Pacific Time)
            { name: 'Pacific Time (US & Canada)', tzCode: 'PST8PDT,M3.2.0,M11.1.0', location: 'Los Angeles, Vancouver' },

            // UTC−07:00 (Mountain Time)
            { name: 'Mountain Time (US & Canada)', tzCode: 'MST7MDT,M3.2.0,M11.1.0', location: 'Denver, Edmonton' },

            // UTC−06:00 (Central Time)
            { name: 'Central Time (US & Canada)', tzCode: 'CST6CDT,M3.2.0,M11.1.0', location: 'Chicago, Mexico City' },

            // UTC−05:00 (Eastern Time)
            { name: 'Eastern Time (US & Canada)', tzCode: 'EST5EDT,M3.2.0,M11.1.0', location: 'New York, Toronto' },

            // UTC−04:00 (Atlantic Time)
            { name: 'Atlantic Time (Canada)', tzCode: 'AST4ADT,M3.2.0,M11.1.0', location: 'Halifax, Puerto Rico' },

            // UTC−03:00
            { name: 'UTC−03:00', tzCode: 'ART3', location: 'Buenos Aires, Greenland' },

            // UTC−02:00
            { name: 'UTC−02:00', tzCode: 'UTC+2', location: 'South Georgia and the South Sandwich Islands' },

            // UTC−01:00 (Azores)
            { name: 'Azores Standard Time', tzCode: 'AZOT1AZOST,M3.5.0/0,M10.5.0/1', location: 'Azores, Cape Verde' },

            // UTC+00:00 (GMT)
            { name: 'Greenwich Mean Time', tzCode: 'GMT0BST,M3.5.0/1,M10.5.0/2', location: 'London, Dublin, Lisbon' },

            // UTC+01:00 (Central European Time)
            { name: 'Central European Time', tzCode: 'CET-1CEST,M3.5.0,M10.5.0/3', location: 'Berlin, Paris, Madrid' },

            // UTC+02:00 (Eastern European Time)
            { name: 'Eastern European Time', tzCode: 'EET-2EEST,M3.5.0/3,M10.5.0/4', location: 'Athens, Cairo, Johannesburg' },

            // UTC+03:00 (Moscow Time)
            { name: 'Moscow Standard Time', tzCode: 'MSK-3', location: 'Moscow, Istanbul, Riyadh' },

            // UTC+04:00
            { name: 'UTC+04:00', tzCode: 'GST-4', location: 'Dubai, Baku, Samara' },

            // UTC+05:00
            { name: 'UTC+05:00', tzCode: 'PKT-5', location: 'Karachi, Islamabad, Tashkent' },

            // UTC+06:00
            { name: 'UTC+06:00', tzCode: 'BST-6', location: 'Dhaka, Almaty' },

            // UTC+07:00
            { name: 'UTC+07:00', tzCode: 'ICT-7', location: 'Bangkok, Jakarta, Hanoi' },

            // UTC+08:00
            { name: 'UTC+08:00', tzCode: 'CST-8', location: 'Beijing, Singapore, Perth' },

            // UTC+09:00
            { name: 'Japan Standard Time', tzCode: 'JST-9', location: 'Tokyo, Seoul, Yakutsk' },

            // UTC+10:00
            { name: 'Australian Eastern Time', tzCode: 'AEST-10AEDT,M10.1.0,M4.1.0/3', location: 'Sydney, Guam, Vladivostok' },

            // UTC+11:00
            { name: 'UTC+11:00', tzCode: 'SBT-11', location: 'Solomon Islands, New Caledonia' },

            // UTC+12:00
            { name: 'New Zealand Standard Time', tzCode: 'NZST-12NZDT,M9.5.0,M4.1.0/3', location: 'Auckland, Fiji, Kamchatka' },

            // UTC+13:00
            { name: 'UTC+13:00', tzCode: 'TOT-13', location: 'Samoa, Tonga' },

            // UTC+14:00
            { name: 'UTC+14:00', tzCode: 'LINT-14', location: 'Kiritimati Island' },
        ];


        // Populate time zone select
        timezones.forEach(tz => {
            const option = document.createElement('option');
            option.value = tz.tzCode; // Use tzCode as the value
            option.textContent = `${tz.name} - ${tz.location}`;
            timezoneSelect.appendChild(option);
        });

        // Event listeners for validation
        ssidInput.addEventListener('input', validateStep1);
        passwordInput.addEventListener('input', validateStep1);

        timezoneSelect.addEventListener('change', validateStep2);

        deviceNameInput.addEventListener('input', validateStep3);

        function validateStep1() {
            if (ssidInput.value.trim() !== '' && passwordInput.value.trim() !== '') {
                next1Button.disabled = false;
            } else {
                next1Button.disabled = true;
            }
        }

        function validateStep2() {
            if (timezoneSelect.value !== '') {
                next2Button.disabled = false;
            } else {
                next2Button.disabled = true;
            }
        }

        function validateStep3() {
            if (deviceNameInput.value.trim() !== '') {
                next3Button.disabled = false;
            } else {
                next3Button.disabled = true;
            }
        }

        // Event listeners for buttons
        next1Button.addEventListener('click', () => goToStep(2));
        next2Button.addEventListener('click', () => goToStep(3));
        next3Button.addEventListener('click', submitSetup);

        back2Button.addEventListener('click', () => goToStep(1));
        back3Button.addEventListener('click', () => goToStep(2));

        function goToStep(step) {
            document.getElementById('step' + currentStep).classList.remove('active');
            currentStep = step;
            document.getElementById('step' + currentStep).classList.add('active');
        }

        function submitSetup() {
            // Collect all the data
            const ssid = ssidInput.value.trim();
            const password = passwordInput.value.trim();
            const timeZoneString = timezoneSelect.value;
            const deviceName = deviceNameInput.value.trim();

            // Disable the next button
            next3Button.disabled = true;

            // Show loading message
            document.querySelector('#step4 .message').textContent = 'Configuring your device...';
            goToStep(4);

            // Send the data to the server
            fetch('/setup', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    ssid: ssid,
                    password: password,
                    timeZoneString: timeZoneString,
                    deviceName: deviceName
                })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Setup response:', data);
                if (data.status === 'success') {
                    // Redirect the user after a delay
                    setTimeout(() => {
                        attemptRedirect(deviceName, data.ip);
                    }, 10000); // Wait for 10 seconds
                } else {
                    document.querySelector('#step4 .message').textContent = 'Failed to connect to Wi-Fi. Please check your credentials and try again.';
                }
            })
            .catch(error => {
                console.error('Error during setup:', error);
                document.querySelector('#step4 .message').textContent = 'An error occurred during setup.';
            });
        }

        function attemptRedirect(deviceName, deviceIP) {
            const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent);
            let redirectURL;
            if (isIOS) {
                redirectURL = `http://${deviceName}.local`;
            } else {
                redirectURL = `http://${deviceIP}`;
            }

            // Update the redirect message with the appropriate link
            redirectMessage.classList.remove('hidden');
            redirectLink.href = redirectURL;
            redirectLink.textContent = redirectURL;

            // Display the IP address
            document.getElementById('ipAddress').textContent = deviceIP;

            // Try to redirect
            window.location.href = redirectURL;
        }


    </script>
</body>
</html>
)rawliteral";

#endif
