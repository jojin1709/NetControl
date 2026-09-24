#include "L10n.h"

L10n& L10n::instance() {
    static L10n i;
    return i;
}

L10n::L10n() { loadEnglish(); }

void L10n::loadEnglish() {
    map_[L"Overview"] = L"Overview";
    map_[L"Applications"] = L"Applications";
    map_[L"Live Monitor"] = L"Live Monitor";
    map_[L"Network Usage"] = L"Network Usage";
    map_[L"Connection Logs"] = L"Connection Logs";
    map_[L"Rules"] = L"Rules";
    map_[L"Settings"] = L"Settings";
    map_[L"Search applications..."] = L"Search applications...";
    map_[L"Sort: Data usage"] = L"Sort: Data usage";
    map_[L"Sort: Name"] = L"Sort: Name";
    map_[L"Sort: Status"] = L"Sort: Status";
    map_[L"Observed Data"] = L"Observed Data";
    map_[L"Current Speed"] = L"Current Speed";
    map_[L"Detected Processes"] = L"Detected Processes";
    map_[L"Blocked Apps"] = L"Blocked Apps";
    map_[L"Allow"] = L"Allow";
    map_[L"Block"] = L"Block";
    map_[L"Ask"] = L"Ask";
    map_[L"Allowed"] = L"Allowed";
    map_[L"Blocked"] = L"Blocked";
    map_[L"Open file"] = L"Open file";
    map_[L"Connections"] = L"Connections";
    map_[L"Protection Active"] = L"Protection Active";
    map_[L"Monitoring real processes"] = L"Monitoring real processes";
    map_[L"Select an application"] = L"Select an application";
    map_[L"Internet Access"] = L"Internet Access";
    map_[L"Path"] = L"Path";
    map_[L"About"] = L"About";
    map_[L"Clear history"] = L"Clear history";
    map_[L"Export rules"] = L"Export rules";
    map_[L"Import rules"] = L"Import rules";
    map_[L"Auto-start"] = L"Auto-start with Windows";
    map_[L"Dark mode"] = L"Dark mode";
    map_[L"Language"] = L"Language";
    map_[L"Save"] = L"Save";
    map_[L"Cancel"] = L"Cancel";
    map_[L"Delete"] = L"Delete";
    map_[L"Enable"] = L"Enable";
    map_[L"Disable"] = L"Disable";
    map_[L"No rules"] = L"No NetControl firewall rules";
    map_[L"No logs"] = L"No connection logs yet";
    map_[L"Copy path"] = L"Copy path";
    map_[L"Select all"] = L"Select all";
    map_[L"Block selected"] = L"Block selected";
    map_[L"Allow selected"] = L"Allow selected";
    map_[L"Language Name"] = L"English";
}

void L10n::loadGerman() {
    loadEnglish();
    map_[L"Overview"] = L"\u00dcbersicht";
    map_[L"Applications"] = L"Anwendungen";
    map_[L"Live Monitor"] = L"Live-Monitor";
    map_[L"Network Usage"] = L"Netzwerknutzung";
    map_[L"Connection Logs"] = L"Verbindungsprotokolle";
    map_[L"Rules"] = L"Regeln";
    map_[L"Settings"] = L"Einstellungen";
    map_[L"Search applications..."] = L"Anwendungen suchen...";
    map_[L"Observed Data"] = L"Beobachtete Daten";
    map_[L"Current Speed"] = L"Aktuelle Geschwindigkeit";
    map_[L"Detected Processes"] = L"Erkannte Prozesse";
    map_[L"Blocked Apps"] = L"Blockierte Apps";
    map_[L"Allow"] = L"Erlauben";
    map_[L"Block"] = L"Blockieren";
    map_[L"Ask"] = L"Fragen";
    map_[L"Allowed"] = L"Erlaubt";
    map_[L"Blocked"] = L"Blockiert";
    map_[L"Open file"] = L"Datei \u00f6ffnen";
    map_[L"Connections"] = L"Verbindungen";
    map_[L"Protection Active"] = L"Schutz aktiv";
    map_[L"Monitoring real processes"] = L"Echte Prozesse werden \u00fcberwacht";
    map_[L"Select an application"] = L"Anwendung ausw\u00e4hlen";
    map_[L"Internet Access"] = L"Internetzugang";
    map_[L"Path"] = L"Pfad";
    map_[L"About"] = L"\u00dcber";
    map_[L"Clear history"] = L"Verlauf l\u00f6schen";
    map_[L"Export rules"] = L"Regeln exportieren";
    map_[L"Import rules"] = L"Regeln importieren";
    map_[L"Auto-start"] = L"Autostart mit Windows";
    map_[L"Dark mode"] = L"Dunkelmodus";
    map_[L"Language"] = L"Sprache";
    map_[L"Save"] = L"Speichern";
    map_[L"Cancel"] = L"Abbrechen";
    map_[L"Delete"] = L"L\u00f6schen";
    map_[L"Enable"] = L"Aktivieren";
    map_[L"Disable"] = L"Deaktivieren";
    map_[L"No rules"] = L"Keine NetControl-Firewallregeln";
    map_[L"No logs"] = L"Noch keine Verbindungsprotokolle";
    map_[L"Copy path"] = L"Pfad kopieren";
    map_[L"Select all"] = L"Alle ausw\u00e4hlen";
    map_[L"Block selected"] = L"Auswahl blockieren";
    map_[L"Allow selected"] = L"Auswahl erlauben";
    map_[L"Language Name"] = L"Deutsch";
}

void L10n::loadSpanish() {
    loadEnglish();
    map_[L"Overview"] = L"Resumen";
    map_[L"Applications"] = L"Aplicaciones";
    map_[L"Live Monitor"] = L"Monitor en vivo";
    map_[L"Network Usage"] = L"Uso de red";
    map_[L"Connection Logs"] = L"Registros de conexi\u00f3n";
    map_[L"Rules"] = L"Reglas";
    map_[L"Settings"] = L"Configuraci\u00f3n";
    map_[L"Search applications..."] = L"Buscar aplicaciones...";
    map_[L"Observed Data"] = L"Datos observados";
    map_[L"Current Speed"] = L"Velocidad actual";
    map_[L"Detected Processes"] = L"Procesos detectados";
    map_[L"Blocked Apps"] = L"Apps bloqueadas";
    map_[L"Allow"] = L"Permitir";
    map_[L"Block"] = L"Bloquear";
    map_[L"Ask"] = L"Preguntar";
    map_[L"Allowed"] = L"Permitido";
    map_[L"Blocked"] = L"Bloqueado";
    map_[L"Open file"] = L"Abrir archivo";
    map_[L"Connections"] = L"Conexiones";
    map_[L"Protection Active"] = L"Protecci\u00f3n activa";
    map_[L"Monitoring real processes"] = L"Monitoreando procesos reales";
    map_[L"Select an application"] = L"Seleccione una aplicaci\u00f3n";
    map_[L"Internet Access"] = L"Acceso a Internet";
    map_[L"Path"] = L"Ruta";
    map_[L"About"] = L"Acerca de";
    map_[L"Clear history"] = L"Borrar historial";
    map_[L"Export rules"] = L"Exportar reglas";
    map_[L"Import rules"] = L"Importar reglas";
    map_[L"Auto-start"] = L"Inicio autom\u00e1tico";
    map_[L"Dark mode"] = L"Modo oscuro";
    map_[L"Language"] = L"Idioma";
    map_[L"Save"] = L"Guardar";
    map_[L"Cancel"] = L"Cancelar";
    map_[L"Delete"] = L"Eliminar";
    map_[L"Enable"] = L"Habilitar";
    map_[L"Disable"] = L"Deshabilitar";
    map_[L"No rules"] = L"Sin reglas de firewall";
    map_[L"No logs"] = L"A\u00fan no hay registros";
    map_[L"Copy path"] = L"Copiar ruta";
    map_[L"Select all"] = L"Seleccionar todo";
    map_[L"Block selected"] = L"Bloquear seleccionados";
    map_[L"Allow selected"] = L"Permitir seleccionados";
    map_[L"Language Name"] = L"Espa\u00f1ol";
}

void L10n::setLanguage(const std::wstring& lang) {
    lang_ = lang;
    map_.clear();
    if (lang == L"de") loadGerman();
    else if (lang == L"es") loadSpanish();
    else loadEnglish();
}

const std::wstring& L10n::tr(const wchar_t* key) const {
    auto it = map_.find(key);
    if (it != map_.end()) return it->second;
    fallback_ = key;
    return fallback_;
}

std::wstring L10n::format(const wchar_t* key, const std::wstring& a) const {
    std::wstring s = tr(key);
    auto p = s.find(L"{}");
    if (p != std::wstring::npos) s.replace(p, 2, a);
    return s;
}
