#include "../include/tokenizador.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <vector>
#include <string_view>


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
string Tokenizador::pasarAMinuscSinAcentos(const string& str) const {
    static const array<unsigned char, 256> map = []() {
        array<unsigned char, 256> m{};
        for (int i = 0; i < 256; ++i) m[i] = static_cast<unsigned char>(i);
        for (unsigned char c = 'A'; c <= 'Z'; ++c) m[c] = c + ('a' - 'A');

        unsigned char a_vals[] = {0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xE0,0xE1,0xE2,0xE3,0xE4,0xE5};
        for (auto c : a_vals) m[c] = 'a';
        unsigned char e_vals[] = {0xC8,0xC9,0xCA,0xCB,0xE8,0xE9,0xEA,0xEB};
        for (auto c : e_vals) m[c] = 'e';
        unsigned char i_vals[] = {0xCC,0xCD,0xCE,0xCF,0xEC,0xED,0xEE,0xEF};
        for (auto c : i_vals) m[c] = 'i';
        unsigned char o_vals[] = {0xD2,0xD3,0xD4,0xD5,0xD6,0xF2,0xF3,0xF4,0xF5,0xF6};
        for (auto c : o_vals) m[c] = 'o';
        unsigned char u_vals[] = {0xD9,0xDA,0xDB,0xDC,0xF9,0xFA,0xFB,0xFC};
        for (auto c : u_vals) m[c] = 'u';

        m[0xDD] = 'y'; m[0xFD] = 'y'; m[0xFF] = 'y';
        m[0xD1] = 0xF1; m[0xF1] = 0xF1;   // ñ
        m[0xC7] = 'c'; m[0xE7] = 'c';     // ç
        m[0xD0] = 'd'; m[0xF0] = 'd';     // eth
        m[0xD8] = 'o'; m[0xF8] = 'o';     // ø
        return m;
    }();

    string resultado;
    const size_t n = str.size();
    resultado.resize(n);

    const unsigned char* src = reinterpret_cast<const unsigned char*>(str.data());
    unsigned char* dst = reinterpret_cast<unsigned char*>(&resultado[0]);

    size_t i = 0;

    // Desenrollado x8
    for (; i + 7 < n; i += 8) {
        dst[i + 0] = map[src[i + 0]];
        dst[i + 1] = map[src[i + 1]];
        dst[i + 2] = map[src[i + 2]];
        dst[i + 3] = map[src[i + 3]];
        dst[i + 4] = map[src[i + 4]];
        dst[i + 5] = map[src[i + 5]];
        dst[i + 6] = map[src[i + 6]];
        dst[i + 7] = map[src[i + 7]];
    }

    // Resto
    for (; i < n; ++i) {
        dst[i] = map[src[i]];
    }

    return resultado;
}

// Se considera delimitador cualquier caracter en delimiters, el espacio en blanco (solo cuando casosEspeciales esté a true) y el salto de línea (\n ó \r) (aunque casosEspeciales esté a false)
bool Tokenizador::esDelimitador (const char& c) const {
    unsigned char uc = static_cast<unsigned char>(c);
    if (casosEspeciales && esSpaceArray[uc]) {
        return true;
    }
    if (c == '\n' || c == '\r') {
        return true;
    }
    return esDelimitadorArray[uc];
}

void Tokenizador::reconstruirTablaDelimitadores() {
    esDelimitadorArray.fill(false);
    for (unsigned char c : delimiters) {
        esDelimitadorArray[c] = true;
    }
}

void Tokenizador::reconstruirTablasAuxiliares() {
    esDigitArray.fill(false);
    esSpaceArray.fill(false);

    for (unsigned char c = '0'; c <= '9'; ++c)
        esDigitArray[c] = true;

    // Espacios clásicos ASCII
    esSpaceArray[' ']  = true;
    esSpaceArray['\t'] = true;
    esSpaceArray['\n'] = true;
    esSpaceArray['\r'] = true;
    esSpaceArray['\v'] = true;
    esSpaceArray['\f'] = true;
}

// URLs 
bool Tokenizador::esDelimitadorCortadorURL (const char& c) const {
    unsigned char uc = static_cast<unsigned char>(c);
    static const array<bool, 256> permitidosURLArray = []() {
        array<bool, 256> arr{};
        arr.fill(false);
        const char* p = "-_.:/?&=#@";
        for (const unsigned char* q = reinterpret_cast<const unsigned char*>(p); *q; ++q) {
            arr[*q] = true;
        }
        return arr;
    }();
    
    return esDelimitador(c) && !permitidosURLArray[uc];
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

    size_t longitudToken = i - start;
    if (longitudToken > 0) {
        string token;
        token.reserve(longitudToken);
        token.append(s.data() + start, longitudToken);
        tokens.push_back(move(token));
    }
    
    return i;
}

// Números decimales
bool Tokenizador::esNumeroDecimal (const string& s, size_t start, size_t end) const {
    bool hayPuntoOComa = false;
    if (!esDelimitadorArray[static_cast<unsigned char>('.')] && !esDelimitadorArray[static_cast<unsigned char>(',')]) {
        return false;
    }
    if ((s[end] != '.' && s[end] != ',')) {
        if (start == 0) {
            return false;
        }
        if ((s[start - 1] != '.' && s[start - 1] != ',')) {
            return false;
        } else {
            hayPuntoOComa = true;
        }
    } else {
        if (end + 1 >= s.size() || !esDigitArray[static_cast<unsigned char>(s[end + 1])]) {
            return false;
        }
    }

    
    char ultimoCaracter = '\0';
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (esDigitArray[static_cast<unsigned char>(c)]) {
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

    char prefijo1 = '\0';
    char prefijo2 = '\0';
    if (i > 0) {
        if (s[i - 1] == '.'){
            prefijo1 = '0';
            prefijo2 = '.';
        } else if (s[i - 1] == ',') {
            prefijo1 = '0';
            prefijo2 = ',';
        }
    }

    char ultimoCaracter = '\0';
    while (i < s.size()) {
        char c = s[i];
        
        if (esDigitArray[static_cast<unsigned char>(c)]) {
            ultimoCaracter = c;
            ++i;
            continue;
        }
        
        if (c == '.' || c == ',') {
            if (ultimoCaracter == '.' || ultimoCaracter == ',') {
                break;
            }
            
            if (i + 1 >= s.size() || !esDigitArray[static_cast<unsigned char>(s[i + 1])]) {
                break;  // No incluir este punto/coma
            }
            
            ultimoCaracter = c;
            ++i;
            continue;
        }
        
        if (c == '%' || c == '$') {
            if (i + 1 >= s.size() || esSpaceArray[static_cast<unsigned char>(s[i + 1])]) {
                break;
            }
        }
        
        if (esDelimitador(c)) {
            break;
        }
        
        break;
    }

    size_t longitudToken = i - start;
    size_t longitudPrefijo = (prefijo1 != '\0') ? 2 : 0;
    string token;

    token.reserve(longitudPrefijo + longitudToken);
    if (prefijo1 != '\0'){
        token.push_back(prefijo1);
        token.push_back(prefijo2);
    }

    token.append(s.data() + start, longitudToken);
    if (!token.empty()) {
        tokens.push_back(move(token));
    }
    
    return i;
}

// E-mails
bool Tokenizador::esEmail (const string& s, size_t start, size_t end) const {
    if (!esDelimitadorArray[static_cast<unsigned char>('@')]) {
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

    size_t longitudToken = i - start;
    if (longitudToken > 0) {
        string token;
        token.reserve(longitudToken);
        token.append(s.data() + start, longitudToken);
        tokens.push_back(move(token));
    }

    return i;
}

// Acrónimos
bool Tokenizador::esAcronimo (const string& s, size_t start, size_t end) const {
    if (!esDelimitadorArray[static_cast<unsigned char>('.')]) {
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

    size_t longitudToken = finToken - inicioToken;
    if (longitudToken > 0) {
        string token;
        token.reserve(longitudToken);
        token.append(s.data() + inicioToken, longitudToken);
        tokens.push_back(move(token));
    }

    return i;
}

// Multipalabras
bool Tokenizador::esGuionMultipalabra (const string& s, size_t start, size_t end) const {
    if (!esDelimitadorArray[static_cast<unsigned char>('-')]) {
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

    size_t longitudToken = i - start;
    if (longitudToken > 0) {
        string token;
        token.reserve(longitudToken);
        token.append(s.data() + start, longitudToken);
        tokens.push_back(move(token));
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

    reconstruirTablaDelimitadores();
    reconstruirTablasAuxiliares();
}

Tokenizador::Tokenizador (const Tokenizador& t) {
    *this = t;
}

Tokenizador::Tokenizador(){
    delimiters=",;:.-/+*\\ '\"{}[]()<>?!??&#=\t@";
    casosEspeciales = true;
    pasarAminuscSinAcentos = false;
    reconstruirTablaDelimitadores();
    reconstruirTablasAuxiliares();
}

Tokenizador::~Tokenizador (){
    delimiters = "";
}

Tokenizador& Tokenizador::operator= (const Tokenizador& t){
    if (this != &t){
        delimiters = t.delimiters;
        casosEspeciales = t.casosEspeciales;
        pasarAminuscSinAcentos = t.pasarAminuscSinAcentos;
        esDelimitadorArray = t.esDelimitadorArray;
    }
    return *this;
}

// Algoritmo de tokenizacion
void Tokenizador::Tokenizar (const string& str, list<string>& tokens) const {
    tokens.clear();

    const string* ps = &str;
    string sConvertida;
    
    if (pasarAminuscSinAcentos) {
        sConvertida = pasarAMinuscSinAcentos(str);
        ps = &sConvertida;
    }
    
    const string& s = *ps;

    const unsigned char* data = reinterpret_cast<const unsigned char*>(s.data());
    const size_t n = s.size();
    const auto& delimArr = esDelimitadorArray;
    const auto& spaceArr = esSpaceArray;

    auto esDelimFast = [&](unsigned char c) -> bool {
        if (spaceArr[c]) return true;
        if (c == '\n' || c == '\r') return true;
        return delimArr[c];
    };

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
        for (size_t i = 0; i < n; ) {
            if (esDelimFast(data[i])) {
                ++i;
                continue;
            }
            size_t start = i;
            if (i > 0 && (s[i - 1] == '.' || s[i - 1] == ',')) {
                if (i > 1 && (s[i - 2] == '.' || s[i - 2] == ',')) {
                    // No es un número decimal si hay dos puntos/comas seguidos
                } else {
                    if (esNumeroDecimal(s, start, i)) {
                        i = procesarNumeroDecimal(s, start, i, tokens);
                        continue;
                    }
                }
            }

            bool casoEspecialEncontrado = false;

            while (i < n && !esDelimFast(data[i])) {
                ++i;
            }
            if (i == n) {
                if (i > start) {
                    tokens.emplace_back(s.data() + start, i - start);
                }
                break;
            }
            unsigned char d = data[i];
            switch (d) {
                case '@':
                    if (esEmail(s, start, i)) {
                        i = procesarEmail(s, start, i, tokens);
                        casoEspecialEncontrado = true;
                    }
                    break;
                case '.':
                    // Prioridad: decimal antes que acrónimo
                    if (esNumeroDecimal(s, start, i)) {
                        i = procesarNumeroDecimal(s, start, i, tokens);
                        casoEspecialEncontrado = true;
                    }
                    else if (esAcronimo(s, start, i)) {
                        i = procesarAcronimo(s, start, i, tokens);
                        casoEspecialEncontrado = true;
                    }
                    break;
                case ',':
                    if (esNumeroDecimal(s, start, i)) {
                        i = procesarNumeroDecimal(s, start, i, tokens);
                        casoEspecialEncontrado = true;
                    }
                    break;
                case '-':
                    if (esGuionMultipalabra(s, start, i)) {
                        i = procesarGuionMultipalabra(s, start, i, tokens);
                        casoEspecialEncontrado = true;
                    }
                    break;
                default:
                    // Solo intentamos URL si este delimitador puede activarla
                    static const array<bool, 256> urlDelimArray = []() {
                        array<bool, 256> arr{};
                        arr.fill(false);
                        const char* p = "_:/.?&-=#@";
                        for (const unsigned char* q =
                             reinterpret_cast<const unsigned char*>(p);
                             *q; ++q) {
                            arr[*q] = true;
                        }
                        return arr;
                    }();
                    if (urlDelimArray[d] && esURL(s, start, i)) {
                        i = procesarURL(s, start, i, tokens);
                        casoEspecialEncontrado = true;
                    }
                    break;
            }
            
            if (!casoEspecialEncontrado) {
                if (i > start) {
                    tokens.emplace_back(s.data() + start, i - start);
                }
                ++i;
            }
        }
    }
}

bool Tokenizador::Tokenizar (const string& i, const string& f) const {
    ifstream entrada(i.c_str(), ios::binary);
    if (!entrada) { 
        cerr << "ERROR: No existe el archivo: " << i << "\n";
        return false; 
    }
    ofstream salida(f.c_str(), ios::binary);
    if (!salida) { 
        cerr << "ERROR: No se ha podido crear el archivo: " << f << "\n";
        return false; 
    }
    string cadena;
    list<string> tokens;
    
    while (getline(entrada, cadena)) {
        if (!cadena.empty()) {
            Tokenizar(cadena, tokens);
            for (const auto& t : tokens) {
                salida << t << "\n";
            }
        }
    }

    return true;
}

bool Tokenizador::Tokenizar (const string& i) const {
    return Tokenizar(i, i + ".tk");
}

bool Tokenizador::TokenizarListaFicheros (const string& i) const {
    ifstream entrada(i.c_str(), ios::binary);
    string nombreFichero;
    bool exito = true;

    if(!entrada) { 
        cerr << "ERROR: No existe el archivo: " << i << "\n"; 
        return false; 
    } 
    
    vector<char> buffer(8 * 1024);
    // Se le reserva el doble del tamaño del buffer para evitar realocaciones   
    string carry;
    carry.reserve(buffer.size() * 2);

    // Funcion lambda para procesar cada línea
    auto procesarLinea = [&](string_view sv) {
        if (sv.empty()) return;
        
        // Quitar '\r' final (Windows CRLF)
        if (!sv.empty() && sv.back() == '\r') {
            sv.remove_suffix(1);
            if (sv.empty()) return;
        }

        string nombreFichero(sv);
    
        struct stat info;
        int err = stat(nombreFichero.c_str(), &info);
    
        if (err == -1) {
            cerr << "ERROR: No existe el archivo: " << nombreFichero << "\n";
            exito = false;
        } else if (S_ISDIR(info.st_mode)) {
            cerr << "ERROR: Es un directorio: " << nombreFichero << "\n";
            exito = false;
        } else {
            if (!Tokenizar(nombreFichero)) {
                exito = false;
            }
        }
    };

    while (true) {
        entrada.read(buffer.data(), static_cast<streamsize>(buffer.size())); // Lee un bloque de tamaño buffer.size() y lo almacena en buffer
        streamsize bytesLeidos = entrada.gcount();
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

            string_view sv(carry.data() + start, pos - start);
            procesarLinea(sv);
            start = pos + 1;
        }
    }

    if (!carry.empty()) {
        string_view sv(carry);
        procesarLinea(sv);
    }

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
    reconstruirTablaDelimitadores();
}

void Tokenizador::AnyadirDelimitadoresPalabra(const string& nuevoDelimiters) {
    delimiters = filtrarDelimitadoresRepetidos(delimiters + nuevoDelimiters);
    reconstruirTablaDelimitadores();
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