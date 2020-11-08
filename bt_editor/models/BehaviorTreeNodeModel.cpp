#include "BehaviorTreeNodeModel.hpp"
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QFile>
#include <QApplication>
#include <QJsonDocument>
#include <QBitmap>

const int MARGIN = 10;
const int DEFAULT_LINE_WIDTH  = 100;
const int DEFAULT_FIELD_WIDTH = 50;
const int DEFAULT_LABEL_WIDTH = 50;

BehaviorTreeDataModel::BehaviorTreeDataModel(const NodeModel &model):
    _uid(GetUID()),
    _model(model),
    _style_caption_color( QtNodes::NodeStyle().FontColor ),
    _style_caption_alias( model.registration_ID )
{
    loadInteractive();
    this->_nodeStyle.LeftIcon = loadIcon();



}

QPixmap BehaviorTreeDataModel::loadIcon(){
    // Load the icon

    QSvgRenderer svgRenderer(_style_icon);
    QPixmap pix(svgRenderer.defaultSize());
    pix.fill( Qt::transparent );
    pix.setMask( pix.createMaskFromColor(_style_caption_color ) );
    QPainter pixPainter( &pix );
    svgRenderer.render( &pixPainter );

    pixPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    pixPainter.fillRect(pix.rect(), _style_caption_color);

    return pix.scaled(24, 24);
}


static std::map<PortDirection, QString> portStrings = {
        {PortDirection::INOUT, "IN-OUT"},
        {PortDirection::INPUT, "INPUT"},
        {PortDirection::OUTPUT, "OUTPUT"},
};

void BehaviorTreeDataModel::loadInteractive(){
    readStyle();

        _nodeStyle.Descriptions.push_back(this->_instance_name);
        _nodeStyle.Descriptions.emplace_back("");

        for(const auto& port_it: _model.ports) {
            auto &direction = portStrings[port_it.second.direction];
            _nodeStyle.Descriptions
                    .emplace_back("[" + direction + "]" + port_it.first + ": [" + port_it.second.default_value + "]");
        }
}



BehaviorTreeDataModel::~BehaviorTreeDataModel(){

}

BT::NodeType BehaviorTreeDataModel::nodeType() const
{
    return _model.type;
}

void BehaviorTreeDataModel::initWidget()
{
    updateNodeSize();
}

unsigned int BehaviorTreeDataModel::nPorts(QtNodes::PortType portType) const
{
    if( portType == QtNodes::PortType::Out)
    {
        if( nodeType() == NodeType::ACTION || nodeType() == NodeType::CONDITION )
        {
            return 0;
        }
        else{
            return 1;
        }
    }
    else if( portType == QtNodes::PortType::In )
    {
        return (_model.registration_ID == "Root") ? 0 : 1;
    }
    return 0;
}

NodeDataModel::ConnectionPolicy BehaviorTreeDataModel::portOutConnectionPolicy(QtNodes::PortIndex) const
{
    return ( nodeType() == NodeType::DECORATOR || _model.registration_ID == "Root") ? ConnectionPolicy::One : ConnectionPolicy::Many;
}

void BehaviorTreeDataModel::updateNodeSize()
{
    // TODO
    emit embeddedWidgetSizeUpdated();
}

QtNodes::NodeDataType BehaviorTreeDataModel::dataType(QtNodes::PortType, QtNodes::PortIndex) const
{
    return NodeDataType {"", ""};
}

std::shared_ptr<QtNodes::NodeData> BehaviorTreeDataModel::outData(QtNodes::PortIndex)
{
    return nullptr;
}

void BehaviorTreeDataModel::readStyle()
{
    QFile style_file(":/NodesStyle.json");

    if (!style_file.open(QIODevice::ReadOnly))
    {
        qWarning("Couldn't open NodesStyle.json");
        return;
    }

    QByteArray bytearray =  style_file.readAll();
    style_file.close();
    QJsonParseError error;
    QJsonDocument json_doc( QJsonDocument::fromJson( bytearray, &error ));

    if(json_doc.isNull()){
        qDebug()<<"Failed to create JSON doc: " << error.errorString();
        return;
    }
    if(!json_doc.isObject()){
        qDebug()<<"JSON is not an object.";
        return;
    }

    QJsonObject toplevel_object = json_doc.object();

    if(toplevel_object.isEmpty()){
        qDebug()<<"JSON object is empty.";
        return;
    }
    QString model_type_name( QString::fromStdString(toStr(_model.type)) );

    for (const auto& model_name: { model_type_name, _model.registration_ID} )
    {
        if( toplevel_object.contains(model_name) )
        {
            auto category_style = toplevel_object[ model_name ].toObject() ;
            if( category_style.contains("icon"))
            {
                _style_icon = category_style["icon"].toString();
            }
            if( category_style.contains("caption_color"))
            {
                _style_caption_color = category_style["caption_color"].toString();
            }
            if( category_style.contains("caption_alias"))
            {
                _style_caption_alias = category_style["caption_alias"].toString();
            }
        }
    }
}

const QString& BehaviorTreeDataModel::registrationName() const
{
    return _model.registration_ID;
}

const QString &BehaviorTreeDataModel::instanceName() const
{
    return _instance_name;
}

PortsMapping BehaviorTreeDataModel::getCurrentPortMapping() const
{
    PortsMapping out;

    return out;
}

QJsonObject BehaviorTreeDataModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"]  = registrationName();
    modelJson["alias"] = instanceName();


    return modelJson;
}

void BehaviorTreeDataModel::restore(const QJsonObject &modelJson)
{
    if( registrationName() != modelJson["name"].toString() )
    {
        throw std::runtime_error(" error restoring: different registration_name");
    }
    QString alias = modelJson["alias"].toString();
    setInstanceName( alias );

    for(auto it = modelJson.begin(); it != modelJson.end(); it++ )
    {
        if( it.key() != "alias" && it.key() != "name")
        {
            setPortMapping( it.key(), it.value().toString() );
        }
    }

}

void BehaviorTreeDataModel::lock(bool locked)
{

}

void BehaviorTreeDataModel::setPortMapping(const QString &port_name, const QString &value)
{

}


void BehaviorTreeDataModel::setInstanceName(const QString &name)
{
    _instance_name = name;
    updateNodeSize();
    emit instanceNameChanged();
}



