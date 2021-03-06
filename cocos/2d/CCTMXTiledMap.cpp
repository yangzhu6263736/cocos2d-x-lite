/****************************************************************************
Copyright (c) 2009-2010 Ricardo Quesada
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2011      Zynga Inc.
Copyright (c) 2013-2014 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
#include "2d/CCTMXTiledMap.h"
#include "2d/CCTMXXMLParser.h"
#include "2d/CCTMXLayer.h"
#include "2d/CCSprite.h"
#include "base/ccUTF8.h"
#include "base/ZipUtils.h"
#include "base/base64.h"
#include "platform/CCFileUtils.h"

NS_CC_BEGIN

// implementation TMXTiledMap

TMXTiledMap * TMXTiledMap::create(const std::string& tmxFile)
{
    TMXTiledMap *ret = new (std::nothrow) TMXTiledMap();
    if (ret->initWithTMXFile(tmxFile))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

TMXTiledMap* TMXTiledMap::createWithXML(const std::string& tmxString, const std::string& resourcePath)
{
    TMXTiledMap *ret = new (std::nothrow) TMXTiledMap();
    if (ret->initWithXML(tmxString, resourcePath))
    {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool TMXTiledMap::initWithTMXFile(const std::string& tmxFile)
{
    CCASSERT(tmxFile.size()>0, "TMXTiledMap: tmx file should not be empty");

    _tmxFile = tmxFile;

    setContentSize(Size::ZERO);

    TMXMapInfo *mapInfo = TMXMapInfo::create(tmxFile);

    if (! mapInfo)
    {
        return false;
    }
    CCASSERT( !mapInfo->getTilesets().empty(), "TMXTiledMap: Map not found. Please check the filename.");
    buildWithMapInfo(mapInfo);

    return true;
}

bool TMXTiledMap::initWithXML(const std::string& tmxString, const std::string& resourcePath)
{
    _tmxFile = tmxString;

    setContentSize(Size::ZERO);

    TMXMapInfo *mapInfo = TMXMapInfo::createWithXML(tmxString, resourcePath);
    //edit by yangzhu
    mapInfo->setMapSize(Size(1500, 1500));
    
    CCASSERT( !mapInfo->getTilesets().empty(), "TMXTiledMap: Map not found. Please check the filename.");
    buildWithMapInfo(mapInfo);

    return true;
}

TMXTiledMap::TMXTiledMap()
    :_mapSize(Size::ZERO)
    ,_tileSize(Size::ZERO)        
    ,_tmxFile("")
    , _tmxLayerNum(0)
{
}

TMXTiledMap::~TMXTiledMap()
{
}

// private
TMXLayer * TMXTiledMap::parseLayer(TMXLayerInfo *layerInfo, TMXMapInfo *mapInfo)
{
    TMXTilesetInfo *tileset = tilesetForLayer(layerInfo, mapInfo);
    if (tileset == nullptr)
        return nullptr;
    
    TMXLayer *layer = TMXLayer::create(tileset, layerInfo, mapInfo);

    if (nullptr != layer)
    {
        // tell the layerinfo to release the ownership of the tiles map.
        layerInfo->_ownTiles = false;
        layer->setupTiles();
    }

    return layer;
}

TMXTilesetInfo * TMXTiledMap::tilesetForLayer(TMXLayerInfo *layerInfo, TMXMapInfo *mapInfo)
{
    Size size = layerInfo->_layerSize;
    auto& tilesets = mapInfo->getTilesets();
    if (tilesets.size()>0)
    {
        TMXTilesetInfo* tileset = nullptr;
        for (auto iter = tilesets.crbegin(); iter != tilesets.crend(); ++iter)
        {
            tileset = *iter;
            if (tileset)
            {
                for( int y=0; y < size.height; y++ )
                {
                    for( int x=0; x < size.width; x++ )
                    {
                        int pos = static_cast<int>(x + size.width * y);
                        int gid = layerInfo->_tiles[ pos ];

                        // gid are stored in little endian.
                        // if host is big endian, then swap
                        //if( o == CFByteOrderBigEndian )
                        //    gid = CFSwapInt32( gid );
                        /* We support little endian.*/

                        // FIXME:: gid == 0 --> empty tile
                        if( gid != 0 ) 
                        {
                            // Optimization: quick return
                            // if the layer is invalid (more than 1 tileset per layer) an CCAssert will be thrown later
                            if( (gid & kTMXFlippedMask) >= tileset->_firstGid )
                                return tileset;
                        }
                    }
                }        
            }
        }
    }

    // If all the tiles are 0, return empty tileset
    CCLOG("cocos2d: Warning: TMX Layer '%s' has no tiles", layerInfo->_name.c_str());
    return nullptr;
}

void TMXTiledMap::buildWithMapInfo(TMXMapInfo* mapInfo)
{
    _mapSize = mapInfo->getMapSize();
    _tileSize = mapInfo->getTileSize();
    _mapOrientation = mapInfo->getOrientation();

    _properties = mapInfo->getProperties();

    _tileProperties = mapInfo->getTileProperties();

    int idx = 0;
    int layerCount = 0;

    // remove the layers & object groups added before
    auto oldChildren = getChildren();
    auto childCount = oldChildren.size();
    for(auto i = childCount - 1; i >= 0; i--){
        auto childNode = oldChildren.at(i);
        if (childNode) {
            auto layer = dynamic_cast<TMXLayer*>(childNode);
            auto objGroup = dynamic_cast<TMXObjectGroup*>(childNode);
            if (layer || objGroup) {
                removeChild(childNode);
            }
        }
    }

    auto& children = mapInfo->getAllChildren();
    for (const auto &childInfo : children) {
        TMXLayerInfo* layerInfo = dynamic_cast<TMXLayerInfo*>(childInfo);
        if (layerInfo && layerInfo->_visible) {
            TMXLayer *child = parseLayer(layerInfo, mapInfo);
            if (child == nullptr) {
                idx++;
                continue;
            }
            addChild(child, idx, idx);
            // update content size with the max size
            const Size& childSize = child->getContentSize();
            Size currentSize = this->getContentSize();
            currentSize.width = std::max(currentSize.width, childSize.width);
            currentSize.height = std::max(currentSize.height, childSize.height);
            this->setContentSize(currentSize);

            idx++;
            layerCount++;
        }
        TMXObjectGroupInfo* groupInfo = dynamic_cast<TMXObjectGroupInfo*>(childInfo);
        if (groupInfo) {
            TMXObjectGroup* group = new TMXObjectGroup(groupInfo, mapInfo);
            group->autorelease();
            addChild(group, idx, idx);
            idx++;
        }
    }
    _tmxLayerNum = layerCount;
}

// public
TMXLayer * TMXTiledMap::getLayer(const std::string& layerName) const
{
    CCASSERT(!layerName.empty(), "Invalid layer name!");

    if (layerName.empty()) {
        return nullptr;
    }

    for (auto& child : _children)
    {
        TMXLayer* layer = dynamic_cast<TMXLayer*>(child);
        if(layer)
        {
            if(layerName.compare( layer->getLayerName()) == 0)
            {
                return layer;
            }
        }
    }

    // layer not found
    return nullptr;
}

/**
 *设置格子
 */
void TMXTiledMap::setTilesSource(const std::string &json){
    //    Document m_doc;
    rapidjson::Document _doc;
    _doc.Parse<0>(json.c_str());
    if (_doc.HasParseError()){
        CCLOG("GetParaseError:  %u\n",_doc.GetParseError());
    }
    using rapidjson::Value;
    Value & mapSourceIndexs = _doc["mapSourceIndexs"];
    int _indexs[4] = {-1,-1,-1,-1};
    if (mapSourceIndexs.IsArray()) {
        for (unsigned int i = 0; i < mapSourceIndexs.Size(); ++i) {
            Value & v = mapSourceIndexs[i];
             auto _index = v.GetString();
            _indexs[i] = atoi(_index);
            log("%s", _index);
        }
    }

    int _adds[4] = {-1,-1,-1,-1};//最多加载4个字地图 地图索引范围0-99
    int _removes[4] = {-1,-1,-1,-1};
    int _x = 0;
    for (int i = 0; i < 4; i++) {
        int _index = _indexs[i];
        if (_index < 0) continue;
        if (!(std::find(std::begin(_currentShardMapIndexs), std::end(_currentShardMapIndexs), _index) != std::end(_currentShardMapIndexs))){//in_array
            _adds[_x++] = _index;
        }
    }

    _x = 0;
    for (int i = 0; i < 4; i++) {
        int _index = _currentShardMapIndexs[i];
        if (_index < 0) continue;
        if (!(std::find(std::begin(_indexs), std::end(_indexs), _index) != std::end(_indexs))){//in_array
            _removes[_x++] = _index;
            _currentShardMapIndexs[i] = 0;
        }
    }

    for (int i = 0; i < 4; i++) {
        _currentShardMapIndexs[i] = _indexs[i];
    }
    for (int i = 0; i < 4; i++) {
        //@todo 移除已加载的shardTiles
        //@todo 移除已加载的shardTiles
        int index = _adds[i];
        if (index < 0) continue;
        for (auto& child : _children)
        {
            TMXLayer* layer = dynamic_cast<TMXLayer*>(child);
            if(!layer) continue;
            if(layer->getLayerName().compare(std::string("bg")) == 0) continue;
            layer->dropShardMapTiles(index);
        }
    }

    for (int i = 0; i < 4; i++) {
        //@todo 移除已加载的shardTiles
        int index = _adds[i];
        if (index < 0) continue;
        for (auto& child : _children)
        {
            TMXLayer* layer = dynamic_cast<TMXLayer*>(child);
            if(!layer) continue;
            std::string layerName = layer->getLayerName();
            if(layerName.compare(std::string("bg")) == 0) continue;

            std::string fileName = layerName + "-" +  cocos2d::Value(index).asString() + ".data";

            uint32_t *_tiles = this->readSource(fileName);
            layer->setShardMapTiles(index, _tiles);
        }
    }
}

uint32_t *TMXTiledMap::readSource(const std::string& fileName){
    uint32_t *_tiles = NULL;
//    res/raw-assets/resources/tieldMap/
//    log("readSourcex1:%s", fileName.c_str());
    std::string _fileName = "res/raw-assets/resources/tieldMap/data/"+fileName;
//    std::string _fileName = "resources/tieldMap/data/"+fileName;
    std::string fullName = FileUtils::getInstance()->fullPathForFilename(_fileName);
//    log("readSourcex2:%s", fullName.c_str());
    Data data = FileUtils::getInstance()->getDataFromFile(fullName);
    if (data.isNull()) {
        return NULL;
    }

    const char* charData;
    charData = (const char*)data.getBytes(), data.getSize();
    size_t charLen = strlen(charData);

    std::string text(charData, 0, charLen);
//    std::string currentString = tmxMapInfo->getCurrentString();
    unsigned char *buffer;
    auto len = base64Decode((unsigned char*)text.c_str(), (unsigned int)text.length(), &buffer);
    if (!buffer)
    {
        CCLOG("cocos2d: TiledMap: decode data error");
        return NULL;
    }
    unsigned char *deflated = nullptr;
    ssize_t sizeHint = 150 * 150 * sizeof(unsigned int);
    ssize_t CC_UNUSED inflatedLen = ZipUtils::inflateMemoryWithHint(buffer, len, &deflated, sizeHint);
    CCASSERT(inflatedLen == sizeHint, "inflatedLen should be equal to sizeHint!");
    
    free(buffer);
    buffer = nullptr;
    
    if (!deflated)
    {
        CCLOG("cocos2d: TiledMap: inflate data error");
        return NULL;
    }
    _tiles = reinterpret_cast<uint32_t*>(deflated);
//    for (int i = 0; i < 2222; i++) {
//        int gid = _tiles[i];
//        log("%d-%d", i, gid);
//    }
//

    return _tiles;
}


TMXObjectGroup * TMXTiledMap::getObjectGroup(const std::string& groupName) const
{
    CCASSERT(!groupName.empty(), "Invalid group name!");

    if (groupName.empty()) {
        return nullptr;
    }

    for (auto& child : _children)
    {
        TMXObjectGroup* group = dynamic_cast<TMXObjectGroup*>(child);
        if(group)
        {
            if(groupName.compare( group->getGroupName()) == 0)
            {
                return group;
            }
        }
    }

    // objectGroup not found
    return nullptr;
}

Vector<TMXObjectGroup*> TMXTiledMap::getObjectGroups()
{
    Vector<TMXObjectGroup*> groups;
    for (auto& child : _children)
    {
        TMXObjectGroup* group = dynamic_cast<TMXObjectGroup*>(child);
        if(group)
        {
            groups.pushBack(group);
        }
    }

    return groups;
}

Value TMXTiledMap::getProperty(const std::string& propertyName) const
{
    if (_properties.find(propertyName) != _properties.end())
        return _properties.at(propertyName);
    
    return Value();
}

Value TMXTiledMap::getPropertiesForGID(int GID) const
{
    if (_tileProperties.find(GID) != _tileProperties.end())
        return _tileProperties.at(GID);
    
    return Value();
}

bool TMXTiledMap::getPropertiesForGID(int GID, Value** value)
{
    if (_tileProperties.find(GID) != _tileProperties.end()) {
        *value = &_tileProperties.at(GID);
        return true;
    } else {
        return false;
    }
}

std::string TMXTiledMap::getDescription() const
{
    return StringUtils::format("<TMXTiledMap | Tag = %d, Layers = %d", _tag, static_cast<int>(_children.size()));
}

int TMXTiledMap::getLayerNum()
{
    return _tmxLayerNum;
}

NS_CC_END
