#pragma once

#include <QObject>
#include <QLabel>
#include <QFile>
#include <QLineEdit>
#include <QFormLayout>
#include <QEvent>
#include <nodes/NodeDataModel>
#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <QSvgRenderer>
#include "bt_editor/bt_editor_base.h"
#include "bt_editor/utils.h"

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;

class BehaviorTreeDataModel : public NodeDataModel
{
    Q_OBJECT

public:
    explicit BehaviorTreeDataModel(const NodeModel &model );
    ~BehaviorTreeDataModel() override;

public:

    [[nodiscard]] NodeType nodeType() const;
    virtual void setInstanceName(const QString& name);

public:

    void initWidget();

    virtual unsigned int nPorts(PortType portType) const override;

    [[nodiscard]] ConnectionPolicy portOutConnectionPolicy(PortIndex) const final;

    [[nodiscard]] NodeDataType dataType(PortType , PortIndex ) const final;

    std::shared_ptr<NodeData> outData(PortIndex port) final;

    void setInData(std::shared_ptr<NodeData>, int) final {}

    [[nodiscard]] const QString &registrationName() const;

    [[nodiscard]] const NodeModel &model() const { return _model; }

    [[nodiscard]] QString name() const final { return registrationName(); }

    [[nodiscard]] const QString& instanceName() const;

    [[nodiscard]] PortsMapping getCurrentPortMapping() const;



    [[nodiscard]] QJsonObject save() const override;

    void restore(QJsonObject const &) override;

    void lock(bool locked);

    void setPortMapping(const QString& port_name, const QString& value);

    [[nodiscard]] int UID() const { return _uid; }

    bool eventFilter(QObject *obj, QEvent *event) override;


public slots:

    void updateNodeSize();

    void onHighlightPortValue(QString value);

protected:

    void loadInteractive();

    QString _instance_name;
    int16_t _uid;
    QColor  _style_caption_color;
    QString _style_icon;
    QString  _style_caption_alias;

private:
    const NodeModel _model;

    void readStyle();


signals:
    void parameterUpdated(QString, QWidget*);
    void instanceNameChanged();
    void portValueDoubleChicked(QLineEdit* value_port);


    QPixmap loadIcon();
};