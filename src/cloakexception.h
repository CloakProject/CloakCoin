#ifndef CLOAKEXCEPTION_H
#define CLOAKEXCEPTION_H

#include "main.h"

class CloakException : std::exception
{
public:
    CloakException();
};

class EnigmaException : CloakException
{

};

class EnigmaSignException : EnigmaException
{

};


#endif // CLOAKEXCEPTION_H

