#include <iostream>
#include <string>
#include <list>
#include "tokenizador.h"

using namespace std;

void imprimirListaSTL(const list<string>& cadena)
{
        list<string>::const_iterator itCadena;
        for(itCadena=cadena.begin();itCadena!=cadena.end();itCadena++)
        {
                cout << (*itCadena) << ", ";
        }
        cout << endl;
}

int
main(void)
{
                bool kCasosEspeciales = true, kpasarAminusculas = true;

                list<string> lt1, lt2, lt3;

                Tokenizador a("[]# ", kCasosEspeciales, kpasarAminusculas);


                a.Tokenizar("áéíóú ÁÉÍÓÚ àèìòù ÀÈÌÒÙ", lt1);
                imprimirListaSTL(lt1);


}
