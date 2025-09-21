# How to run

1. Install SEGGER.
2. Open the project in `examples/my_projects/ble_app_template/pca10040/ses/ble_app_template_pca10040_s132.emproject`.
3. Add the following user include directories in the preprocessor tab in the project options (right-click the project and select **Options**):

   ```bash
   ./ble
   ./epd
   ./rtc
   ./power_manager
   ./eeprom
   ```

4. Build and upload
