// WaveToAmiga - Komponenten-Skript des Installers (Qt Installer Framework)
//  * "Programm jetzt starten" auf der Abschlussseite
//  * Verknuepfungen (Windows: Startmenue + Desktop) bzw. Desktop-Eintrag (Linux)
// Der Schritt "Vorhandene Installation deinstallieren" steht im Controller-Skript (config/controller.qs).

function Component() {
    var windows = (systemInfo.kernelType === "winnt");
    installer.setValue("RunProgram", windows ? "@TargetDir@/WaveToAmiga.exe"
                                             : "@TargetDir@/bin/WaveToAmiga");
    installer.setValue("RunProgramDescription", "WaveToAmiga jetzt starten");
}

Component.prototype.createOperations = function () {
    component.createOperations();   // kopiert die Dateien aus data/

    if (systemInfo.kernelType === "winnt") {
        var exe = "@TargetDir@/WaveToAmiga.exe";
        component.addOperation("CreateShortcut", exe, "@StartMenuDir@/WaveToAmiga.lnk",
            "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0",
            "description=WaveToAmiga - WAV in Amiga-Soundformate konvertieren");
        component.addOperation("CreateShortcut", exe, "@DesktopDir@/WaveToAmiga.lnk",
            "workingDirectory=@TargetDir@", "iconPath=" + exe, "iconId=0",
            "description=WaveToAmiga - WAV in Amiga-Soundformate konvertieren");
    } else if (systemInfo.kernelType === "linux") {
        component.addOperation("CreateDesktopEntry", "WaveToAmiga.desktop",
            "Version=1.0\n" +
            "Type=Application\n" +
            "Name=WaveToAmiga\n" +
            "GenericName=WAV to Amiga sound converter\n" +
            "GenericName[de]=WAV-zu-Amiga-Sound-Konverter\n" +
            "Comment=Convert WAV files to Amiga sound formats (8SVX, AIFF, 16SV, MAUD)\n" +
            "Comment[de]=WAV-Dateien in Amiga-Soundformate konvertieren\n" +
            "Exec=@TargetDir@/bin/WaveToAmiga %F\n" +
            "Icon=@TargetDir@/share/icons/hicolor/256x256/apps/WaveToAmiga.png\n" +
            "Terminal=false\n" +
            "Categories=AudioVideo;Audio;AudioVideoEditing;\n" +
            "MimeType=audio/x-wav;audio/wav;audio/vnd.wave;\n" +
            "StartupWMClass=WaveToAmiga");
    }
};
