#include "../include/tokenizador.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <cctype>
#include <vector>


// ============================
// === Funciones auxiliares ===
// ============================
string Tokenizador::filtrarDelimitadoresRepetidos (const string& delimitadoresPalabra) {
    string delimitadoresFiltrados = "";
    for (size_t i = 0; i < delimitadoresPalabra.size(); ++i) {
        if (delimitadoresFiltrados.find(delimitadoresPalabra[i]) == string::npos) { // Si no se encuentra el caracter en delimitadoresFiltrados devuelve string::npos
            delimitadoresFiltrados += delimitadoresPalabra[i];
        }
    }
    return delimitadoresFiltrados;
}

// Método que pasa a minúsculas y quita acentos (ISO-8859-1)
// Cada caracter ocupa un byte en ISO-8859-1
string Tokenizador::pasarAMinuscSinAcentos (const string& str) const{
    string resultado;
    resultado.reserve(str.size());

    for (unsigned char ch : str) {
        // Pasar a minúsculas
        if (ch >= 'A' && ch <= 'Z') {
            resultado.push_back(static_cast<char>(ch + ('a' - 'A')));
            continue;
        }

        switch (ch) {
            // a / A con acentos y variantes (ÀÁÂÃÄÅ àáâãäå)
            case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5:
            case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5:
                resultado.push_back('a');
                break;

            // e / E (ÈÉÊË èéêë)
            case 0xC8: case 0xC9: case 0xCA: case 0xCB:
            case 0xE8: case 0xE9: case 0xEA: case 0xEB:
                resultado.push_back('e');
                break;

            // i / I (ÌÍÎÏ ìíîï)
            case 0xCC: case 0xCD: case 0xCE: case 0xCF:
            case 0xEC: case 0xED: case 0xEE: case 0xEF:
                resultado.push_back('i');
                break;

            // o / O (ÒÓÔÕÖ òóôõö)
            case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6:
            case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6:
                resultado.push_back('o');
                break;

            // u / U (ÙÚÛÜ ùúûü)
            case 0xD9: case 0xDA: case 0xDB: case 0xDC:
            case 0xF9: case 0xFA: case 0xFB: case 0xFC:
                resultado.push_back('u');
                break;

            // y / Y (Ý ý ÿ)
            case 0xDD: case 0xFD: case 0xFF:
                resultado.push_back('y');
                break;

            // ñ / Ñ
            case 0xD1: case 0xF1:
                resultado.push_back('ñ');
                break;

            // ç / Ç
            case 0xC7: case 0xE7:
                resultado.push_back('c');
                break;

            // Ð / ð (eth) -> d
            case 0xD0: case 0xF0:
                resultado.push_back('d');
                break;

            // Ø / ø -> o
            case 0xD8: case 0xF8:
                resultado.push_back('o');
                break;

            default:
                resultado.push_back(static_cast<char>(ch));
                break;
        }
    }

    return resultado;
}

// Se considera delimitador cualquier caracter en delimiters, el espacio en blanco (solo cuando casosEspeciales esté a true) y el salto de línea (\n ó \r) (aunque casosEspeciales esté a false)
bool Tokenizador::esDelimitador (const char& c) const {
    if (casosEspeciales && isspace(static_cast<unsigned char>(c))) {
        return true;
    }
    if (c == '\n' || c == '\r') {
        return true;
    }
    if (delimiters.find(c) != string::npos) {
        return true;
    }
    return false;
}

// URLs 
bool Tokenizador::esDelimitadorCortadorURL (const char& c) const {
    string caracteresPermitidosURL = "-_.:/?&=#@";
    
    if (esDelimitador(c) && caracteresPermitidosURL.find(c) == string::npos) {
        return true;
    }
    return false;
}
bool Tokenizador::esURL (const string& s, size_t start, size_t end) const {
    size_t posicionDespuesPrefijo = 0;
    
    if (s.compare(start, 5, "http:") == 0) {
        posicionDespuesPrefijo = start + 5;
    }
    else if (s.compare(start, 6, "https:") == 0) {
        posicionDespuesPrefijo = start + 6;
    }
    else if (s.compare(start, 4, "ftp:") == 0) {
        posicionDespuesPrefijo = start + 4;
    }
    else {
        return false;
    }
    
    if (posicionDespuesPrefijo >= s.size()) {
        return false;
    }
    
    if (esDelimitadorCortadorURL(s[posicionDespuesPrefijo])) {
        return false;
    }
    
    return true;
}
size_t Tokenizador::procesarURL (const string& s, size_t start, size_t end, list<string>& tokens) const {
    size_t i = start;
    
    while (i < s.size() && !esDelimitadorCortadorURL(s[i])) {
        ++i;
    }
    
    string token = s.substr(start, i - start);
    if (!token.empty()) {
        tokens.push_back(token);
    }
    
    return i;
}

// Números decimales
bool Tokenizador::esNumeroDecimal (const string& s, size_t start, size_t end) const {
    // cout << "Comprobando si es número decimal en posición " << start << " con punto/coma en posición " << end << ": " << s.substr(start, end - start) << endl << endl;
    bool hayPuntoOComa = false;
    if (delimiters.find('.') == string::npos && delimiters.find(',') == string::npos) {
        // cout << "No se han definido '.' ni ',' como delimitadores, por lo que no se considerará número decimal" << endl << endl;
        return false;
    }
    if ((s[end] != '.' && s[end] != ',')) {
        // cout << "El carácter en posición " << end << " no es un punto ni una coma" << endl << endl;
        if ((s[start - 1] != '.' && s[start - 1] != ',')) {
            // cout << "El carácter antes del número tampoco es un punto ni una coma, por lo que no se considerará número decimal" << endl << endl;
            return false;
        } else {
            hayPuntoOComa = true;
        }
    } else {
        if (end + 1 >= s.size() || !isdigit(static_cast<unsigned char>(s[end + 1]))) {
            // cout << "El carácter después del punto/coma no es un dígito" << endl << endl;
            return false;
        }
    }

    
    char ultimoCaracter = '\0';
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (isdigit(static_cast<unsigned char>(c))) {
            ultimoCaracter = c;
            continue;
        }
        if (c == '.' || c == ',') {
            if (ultimoCaracter == '.' || ultimoCaracter == ',') {
                return false;
            }
            hayPuntoOComa = true;
            ultimoCaracter = c;
            continue;
        }
        if (esDelimitador(c)) {
            break;
        }
        return false;
    }
    
    return hayPuntoOComa;
}
size_t Tokenizador::procesarNumeroDecimal (const string& s, size_t start, size_t end, list<string>& tokens) const {
    size_t i = start;
    string token = "";

    // cout << "Procesando número decimal desde posición " << start << " con punto/coma en posición " << end << endl;
    // cout << "Caracter antes del número: '" << (start > 0 ? s[start - 1] : ' ') << "'" << endl;
    if (s[i - 1] == '.'){
        token += "0.";
    } else if (s[i - 1] == ',') {
        token += "0,";
    }

    char ultimoCaracter = '\0';
    while (i < s.size()) {
        char c = s[i];
        
        if (isdigit(static_cast<unsigned char>(c))) {
            ultimoCaracter = c;
            ++i;
            continue;
        }
        
        if (c == '.' || c == ',') {
            if (ultimoCaracter == '.' || ultimoCaracter == ',') {
                break;
            }
            
            if (i + 1 >= s.size() || !isdigit(static_cast<unsigned char>(s[i + 1]))) {
                break;  // No incluir este punto/coma
            }
            
            ultimoCaracter = c;
            ++i;
            continue;
        }
        
        if (c == '%' || c == '$') {
            if (i + 1 >= s.size() || isspace(static_cast<unsigned char>(s[i + 1]))) {
                break;
            }
        }
        
        if (esDelimitador(c)) {
            break;
        }
        
        break;
    }
    
    token += s.substr(start, i - start); 
    if (!token.empty()) {
        tokens.push_back(token);
    }
    
    return i;
}

// E-mails
bool Tokenizador::esEmail (const string& s, size_t start, size_t end) const {
    if (delimiters.find('@') == string::npos) {
        return false;
    }
    if (end >= s.size() || s[end] != '@') {
        return false;
    }
    if (end == start) {
        return false;
    }
    if (end + 1 >= s.size()) {
        return false;
    }

    char caracterDespuesArroba = s[end + 1];
    if (esDelimitador(caracterDespuesArroba) && caracterDespuesArroba != '.' && caracterDespuesArroba != '-' && caracterDespuesArroba != '_') {
        return false;
    }

    for (size_t i = start; i < end; ++i) {
        if (s[i] == '@') {
            return false;
        }
    }

    size_t i = end + 1;
    while (i < s.size()) {
        char c = s[i];
        if (c == '@') {
            return false;
        }
        if (esDelimitador(c) && c != '.' && c != '-' && c != '_') {
            break;
        }
        if (c == '.' || c == '-' || c == '_') {
            if (i + 1 >= s.size() || (esDelimitador(s[i + 1]) && s[i + 1] != '.' && s[i + 1] != '-' && s[i + 1] != '_')) {
                break;
            }
        }
        ++i;
    }
    return true;
}
size_t Tokenizador::procesarEmail (const string& s, size_t start, size_t end, list<string>& tokens) const {
    size_t i = end + 1;
    while (i < s.size()) {
        char c = s[i];

        if (esDelimitador(c) && c != '.' && c != '-' && c != '_') {
            break;
        }
        if (c == '.' || c == '-' || c == '_') {
            if (i + 1 >= s.size() || (esDelimitador(s[i + 1]) && s[i + 1] != '.' && s[i + 1] != '-' && s[i + 1] != '_')) {
                break;
            }
        }
        ++i;
    }
    string token = s.substr(start, i - start);
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return i;
}

// Acrónimos
bool Tokenizador::esAcronimo (const string& s, size_t start, size_t end) const {
    if (delimiters.find('.') == string::npos) {
        return false;
    }
    if (end >= s.size() || s[end] != '.') {
        return false;
    }
    if (end == start) {
        return false;
    }
    if (end + 1 >= s.size()) {
        return false;
    }
    if (s[end + 1] == '.') {
        return false;
    }

    char ultimoCaracter = '\0';
    for (size_t i = start; i < end; ++i) {
        char c = s[i];
        if (c == '.') {
            if (ultimoCaracter == '.') {
                return false;
            }
        }
        ultimoCaracter = c;
    }

    return true;
}
size_t Tokenizador::procesarAcronimo (const string& s, size_t start, size_t end, list<string>& tokens) const {
    size_t i = end + 1;
    while (i < s.size()) {
        char c = s[i];
        if (c == '.') {
            if (i + 1 < s.size() && s[i + 1] == '.') {
                break;
            }
            ++i;
            continue;
        }
        if (esDelimitador(c)) {
            break;
        }
        ++i;
    }

    size_t inicioToken = start;
    while (inicioToken < i && s[inicioToken] == '.') {
        inicioToken++;
    }

    size_t finToken = i;
    while (finToken > inicioToken && s[finToken - 1] == '.') {
        finToken--;
    }

    string token = s.substr(inicioToken, finToken - inicioToken);
    if (!token.empty()) {
        tokens.push_back(token);
    }

    return i;
}

// Multipalabras
bool Tokenizador::esGuionMultipalabra (const string& s, size_t start, size_t end) const {
    if (delimiters.find('-') == string::npos) {
        return false;
    }
    if (end >= s.size() || s[end] != '-') {
        return false;
    }
    if (end == start) {
        return false;
    }
    if (end + 1 >= s.size()) {
        return false;
    }
    if (esDelimitador(s[end - 1]) || esDelimitador(s[end + 1])) {
        return false;
    }
    return true;
}
size_t Tokenizador::procesarGuionMultipalabra (const string& s, size_t start, size_t end, list<string>& tokens) const {
    size_t i = end + 1;
    while (i < s.size()) {
        char c = s[i];
        if (c == '-') {
            if (i + 1 >= s.size()) {
                break;
            }
            if (esDelimitador(s[i + 1])) {
                break;
            }
            ++i;
            continue;
        }
        if (esDelimitador(c)) {
            break;
        }
        ++i;
    }
    string token = s.substr(start, i - start);
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return i;
}


// =============================================================
// === Implementación de los métodos de la clase Tokenizador ===
// =============================================================
Tokenizador::Tokenizador(const string& delimitadoresPalabra, const bool& kcasosEspeciales, const bool& minuscSinAcentos){
    delimiters = filtrarDelimitadoresRepetidos(delimitadoresPalabra);
    casosEspeciales = kcasosEspeciales;
    pasarAminuscSinAcentos = minuscSinAcentos;
}

Tokenizador::Tokenizador (const Tokenizador& t) {
    *this = t;
}

Tokenizador::Tokenizador(){
    delimiters=",;:.-/+*\\ '\"{}[]()<>?!??&#=\t@";
    //_:/.?&-=#@
    casosEspeciales = true;
    pasarAminuscSinAcentos = false;
}

Tokenizador::~Tokenizador (){
    delimiters = "";
}

Tokenizador& Tokenizador::operator= (const Tokenizador& t){
    if (this != &t){
        delimiters = t.delimiters;
        casosEspeciales = t.casosEspeciales;
        pasarAminuscSinAcentos = t.pasarAminuscSinAcentos;
    }
    return *this;
}

// Algoritmo de tokenizacion
void Tokenizador::Tokenizar (const string& str, list<string>& tokens) const {
    tokens.clear();

    string s = str;
    if (pasarAminuscSinAcentos) {
        s = pasarAMinuscSinAcentos(str);
    }

    if (!casosEspeciales) {
        // Si no se tratan casos especiales, se tokeniza de forma simple
        string::size_type lastPos = s.find_first_not_of(delimiters,0); 
        string::size_type pos = s.find_first_of(delimiters,lastPos); 
        
        while(string::npos != pos || string::npos != lastPos) 
        { 
            tokens.push_back(s.substr(lastPos, pos - lastPos)); 
            lastPos = s.find_first_not_of(delimiters, pos); 
            pos = s.find_first_of(delimiters, lastPos); 
        }
        return;
    } else {
        
        for (size_t i = 0; i < s.size(); ) {
            if (esDelimitador(s[i])) {
                ++i;
                continue;
            }
            size_t start = i;

            bool casoEspecialEncontrado = false;
            while (i <= s.size()) {

                if (!esDelimitador(s[i]) && i < s.size()) {
                    ++i;
                    continue;
                }

                if (esURL(s, start, i)) {
                    // cout << "Detectada URL en posición " << start << ": " << s.substr(start, i - start) << endl << endl;
                    i = procesarURL(s, start, i, tokens);
                    casoEspecialEncontrado = true;
                }
                else if (esNumeroDecimal(s, start, i)) {
                    // cout << "Detectado número decimal en posición " << start << ": " << s.substr(start, i - start) << endl << endl;
                    i = procesarNumeroDecimal(s, start, i, tokens);
                    casoEspecialEncontrado = true;
                }
                else if (esEmail(s, start, i)) {
                    // cout << "Detectado email en posición " << start << ": " << s.substr(start, i - start) << endl << endl;
                    i = procesarEmail(s, start, i, tokens);
                    casoEspecialEncontrado = true;
                }
                else if (esAcronimo(s, start, i)) {
                    // cout << "Detectado acrónimo en posición " << start << ": " << s.substr(start, i - start) << endl << endl;
                    i = procesarAcronimo(s, start, i, tokens);
                    casoEspecialEncontrado = true;
                }
                else if (esGuionMultipalabra(s, start, i)) {
                    // cout << "Detectada palabra con guion en posición " << start << ": " << s.substr(start, i - start) << endl << endl;
                    i = procesarGuionMultipalabra(s, start, i, tokens);
                    casoEspecialEncontrado = true;
                }
                
                break;
            }

            if (!casoEspecialEncontrado) {
                if (i > start) {
                    string token = s.substr(start, i - start);
                    if (!token.empty()) {
                        tokens.push_back(token);
                    }
                }
            }
        }
    }
}

// Lea un buffer de tamaño addecuado
bool Tokenizador::Tokenizar (const string& i, const string& f) const {
    ifstream entrada(i.c_str(), ios::binary);
    ofstream salida(f.c_str(), ios::binary);
    string cadena;
    list<string> tokens;

    if(!entrada) { 
        cerr << "ERROR: No existe el archivo: " << i << endl; 
        return false; 
    } else { 
        while(getline(entrada, cadena)) {  
            if(cadena.length()!=0) { 
                Tokenizar(cadena, tokens); 
            } 
        } 
    } 
    
    entrada.close(); 
    
    if(!salida) { 
        cerr << "ERROR: No se ha podido crear el archivo: " << f << endl; 
        return false; 
    }
    list<string>::iterator itS; 

    for(itS= tokens.begin(); itS!= tokens.end(); itS++) {   
        salida << (*itS) << endl; 
    } 

    salida.close(); 
    return true;
}

bool Tokenizador::Tokenizar (const string& i) const {
    return Tokenizar(i, i + ".tk");
}


// Modificar para que trate el fichero de texto como binario y que lea de un buffer de tamaño adecuado
bool Tokenizador::TokenizarListaFicheros (const string& i) const {
    ifstream entrada(i.c_str(), ios::binary);
    string nombreFichero;
    bool exito = true;

    if(!entrada) { 
        cerr << "ERROR: No existe el archivo: " << i << endl; 
        return false; 
    } 
    
    vector<char> buffer(1 << 20);
    // Se le reserva el doble del tamaño del buffer para evitar realocaciones   
    string carry;
    carry.reserve(buffer.size() * 2);

    // Funcion lambda para procesar cada línea
    // [&] -> Permite modificar la variable exito del ámbito externo
    auto procesarLinea = [&](string& nombreFichero) {
        // cout << "Procesando fichero: " << nombreFichero << endl;
        if (nombreFichero.empty()) {
            return;
        }

        if(!nombreFichero.empty() && nombreFichero.back() == '\r') {
            nombreFichero.pop_back();
            if (nombreFichero.empty()) {
                return;
            }
        }

        struct stat info;
        // int stat(const char *pathname, struct stat *buf); -> Guarda en buf la informaci?n del archivo con ruta pathname. Devuelve 0 si tiene ?xito y -1 si hay error.
        int err = stat(nombreFichero.c_str(), &info);

        if (err == -1) {
            cerr << "ERROR: No existe el archivo: " << nombreFichero << endl;
            exito = false; 
        }
        else if (S_ISDIR(info.st_mode)) {
            cerr << "ERROR: Es un directorio: " << nombreFichero << endl;
            exito = false; 
        }
        else {
            if (!Tokenizar(nombreFichero)) {
                exito = false; 
            }
        }
    };

    while (true) {
        entrada.read(buffer.data(), static_cast<std::streamsize>(buffer.size())); // Lee un bloque de tamaño buffer.size() y lo almacena en buffer
        std::streamsize bytesLeidos = entrada.gcount();
        if (bytesLeidos <= 0) {
            break;
        }

        carry.append(buffer.data(), static_cast<size_t>(bytesLeidos));

        size_t start = 0;
        while (true) {
            size_t pos = carry.find('\n', start);
            if (pos == string::npos) {
                if (start > 0){
                    carry.erase(0, start);
                }
                break;
            }

            string nombreFichero = carry.substr(start, pos - start);
            procesarLinea(nombreFichero);
            // cout << "Línea procesada: " << nombreFichero << endl;
            start = pos + 1;
        }
    }

    if (!carry.empty()) {
        procesarLinea(carry);
    }

    // while(getline(entrada, nombreFichero)) {
    //     if (nombreFichero.empty()) {
    //         continue;
    //     }
    //     struct stat info;
    //     // int stat(const char *pathname, struct stat *buf); -> Guarda en buf la informaci?n del archivo con ruta pathname. Devuelve 0 si tiene ?xito y -1 si hay error.
    //     int err = stat(nombreFichero.c_str(), &info);

    //     if (err == -1) {
    //         cerr << "ERROR: No existe el archivo: " << nombreFichero << endl;
    //         exito = false; 
    //     }
    //     else if (S_ISDIR(info.st_mode)) {
    //         cerr << "ERROR: Es un directorio: " << nombreFichero << endl;
    //         exito = false; 
    //     }
    //     else {
    //         if (!Tokenizar(nombreFichero)) {
    //             exito = false; 
    //         }
    //     }
    // }

    entrada.close();
    return exito;
}


bool Tokenizador::TokenizarDirectorio (const string& i) const {
    struct stat dir;

    int err = stat(i.c_str(), &dir);   
    if(err == -1 || !S_ISDIR(dir.st_mode)) 
        return false; 
    else { 
        // Hago una lista en un fichero con find>fich  
        string cmd = "find " + i + " -follow |sort > .lista_fich"; 
        system(cmd.c_str()); 
        return TokenizarListaFicheros(".lista_fich"); 
    }
}

void Tokenizador::DelimitadoresPalabra(const string& nuevoDelimiters) {
    delimiters = filtrarDelimitadoresRepetidos(nuevoDelimiters);
}

void Tokenizador::AnyadirDelimitadoresPalabra(const string& nuevoDelimiters) {
    delimiters = filtrarDelimitadoresRepetidos(delimiters + nuevoDelimiters);
}

string Tokenizador::DelimitadoresPalabra() const {
    return delimiters;
}

void Tokenizador::CasosEspeciales (const bool& nuevoCasosEspeciales) {
    casosEspeciales = nuevoCasosEspeciales;
}

bool Tokenizador::CasosEspeciales () {
    return casosEspeciales;
}

void Tokenizador::PasarAminuscSinAcentos (const bool& nuevoPasarAminuscSinAcentos) {
    pasarAminuscSinAcentos = nuevoPasarAminuscSinAcentos;
}

bool Tokenizador::PasarAminuscSinAcentos () {
    return pasarAminuscSinAcentos;
}

ostream& operator<<(ostream& os, const Tokenizador& t) {
    os << "DELIMITADORES: " << t.delimiters << " TRATA CASOS ESPECIALES: " << t.casosEspeciales << " PASAR A MINUSCULAS Y SIN ACENTOS: " << t.pasarAminuscSinAcentos;
    return os;
}