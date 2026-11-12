// WaveToAmiga - Controller-Skript des Installers (Qt Installer Framework)
//
// Schritt "Vorhandene Installation": Sobald die Willkommensseite erscheint, wird nach einer bereits
// installierten Version von WaveToAmiga gesucht. Wird eine gefunden, kann sie auf Wunsch zuerst deinstalliert
// werden (ueber das Wartungswerkzeug "WaveToAmiga-Deinstallieren", Konsolenmodus "purge").
//
// Wichtig: Alles ist in try/catch gekapselt. Ein Fehler hier darf den Installer nie am Starten hindern.

function Controller() {
    this.uninstallOffered = false;     // die Frage nur einmal stellen (Callback wird auch beim Zurueckgehen aufgerufen)
}

function isWindows() {
    return systemInfo.kernelType === "winnt";
}

function maintenanceToolPath(dir) {
    return dir + "/" + installer.value("MaintenanceToolName") + (isWindows() ? ".exe" : "");
}

// Sucht eine vorhandene Installation: zuerst im vorgeschlagenen Zielordner, dann an ueblichen Orten.
function findExistingInstallation() {
    var candidates = [installer.value("TargetDir")];
    if (isWindows()) {
        candidates.push("C:/WaveToAmiga");
    } else {
        candidates.push(installer.value("HomeDir") + "/WaveToAmiga");
        candidates.push("/opt/WaveToAmiga");
    }
    for (var i = 0; i < candidates.length; ++i) {
        if (candidates[i] && installer.fileExists(maintenanceToolPath(candidates[i])))
            return candidates[i];
    }
    return "";
}

function pause(seconds) {
    if (isWindows())
        installer.execute("ping", ["-n", String(seconds + 1), "127.0.0.1"]);
    else
        installer.execute("sleep", [String(seconds)]);
}

Controller.prototype.IntroductionPageCallback = function () {
    if (this.uninstallOffered)
        return;
    this.uninstallOffered = true;

    try {
        var dir = findExistingInstallation();
        if (dir === "")
            return;                                   // nichts installiert -> nichts zu tun

        var tool = maintenanceToolPath(dir);
        var answer = QMessageBox["question"]("wavetoamiga.uninstall", "Vorhandene Installation",
            "Auf diesem Computer ist bereits WaveToAmiga installiert:\n\n" + dir + "\n\n" +
            "Soll die vorhandene Version jetzt deinstalliert werden? (empfohlen)\n\n" +
            "Mit \u201eNein\u201c koennen Sie im naechsten Schritt auch einen anderen Installationsordner waehlen.",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.Yes);
        if (answer !== QMessageBox.Yes)
            return;

        // Wartungswerkzeug im Konsolenmodus: alles entfernen, ohne weitere Rueckfrage
        installer.execute(tool, ["purge", "--confirm-command"]);

        // Unter Windows startet sich das Werkzeug zum Selbstloeschen aus einem Temp-Ordner neu -> warten (max. 90 s)
        for (var i = 0; i < 90 && installer.fileExists(tool); ++i)
            pause(1);

        if (installer.fileExists(tool)) {
            QMessageBox["warning"]("wavetoamiga.uninstall.failed", "Deinstallation",
                "Die vorhandene Installation konnte nicht vollstaendig entfernt werden.\n\n" +
                "Bitte starten Sie\n" + tool + "\nmanuell und fuehren Sie dieses Setup danach erneut aus.",
                QMessageBox.Ok);
        } else {
            QMessageBox["information"]("wavetoamiga.uninstall.done", "Deinstallation",
                "Die vorhandene Installation wurde entfernt. Das Setup wird jetzt fortgesetzt.",
                QMessageBox.Ok);
        }
    } catch (e) {
        console.log("WaveToAmiga: Pruefung auf vorhandene Installation uebersprungen: " + e);
    }
};
