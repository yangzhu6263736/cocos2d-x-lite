#include "SDKManager.h"

using namespace anysdk::framework;

SDKManager* SDKManager::_pInstance = NULL;

SDKManager::SDKManager()
{
}

SDKManager::~SDKManager()
{
	_pAgent->unloadAllPlugins();
}

SDKManager* SDKManager::getInstance()
{
    if (_pInstance == NULL) {
        _pInstance = new SDKManager();
    }
    return _pInstance;
}

void SDKManager::purge()
{
    if (_pInstance)
    {
        delete _pInstance;
        _pInstance = NULL;
    }
}

void SDKManager::loadAllPlugins()
{
    /**
     * appKey, appSecret and privateKey are the only three parameters generated 
     * after the packing tool client finishes creating the game.
     * The oauthLoginServer parameter is the API address provided by the game service
     * to login verification
     */
    std::string oauthLoginServer = "http://c.ww.mmgame.net:8081/index.php/privateinterface/anySdkLoginCheck";
    std::string appKey = "9911B089-7D96-4D4F-BD49-31E266CE44D1";
    std::string appSecret = "87373f6344cd8ff4a302e0e713046cbd";
    std::string privateKey = "20CB2F21202E9547EE4D5CAA7600480B";
    
    AgentManager* pAgent = AgentManager::getInstance();
    pAgent->init(appKey,appSecret,privateKey,oauthLoginServer);
    
    //Initialize plug-ins, including SDKs.
    pAgent->loadAllPlugins();
}


