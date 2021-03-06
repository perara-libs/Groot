#include "sidepanel_monitor.h"
#include "ui_sidepanel_monitor.h"
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QLabel>
#include <QDebug>

#include "utils.h"

SidepanelMonitor::SidepanelMonitor(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SidepanelMonitor),
    _zmq_context(1),
    _zmq_subscriber(_zmq_context, ZMQ_SUB),
    _connected(false),
    _msg_count(0)
{
    ui->setupUi(this);
    _timer = new QTimer(this);

    connect( _timer, &QTimer::timeout, this, &SidepanelMonitor::on_timer );
}

SidepanelMonitor::~SidepanelMonitor()
{
    delete ui;
}

void SidepanelMonitor::clear()
{
    if( _connected ) this->on_Connect();
}

void SidepanelMonitor::on_timer()
{
    if( !_connected ) return;

    zmq::message_t msg;
    try{
        while(  _zmq_subscriber.recv(&msg) )
        {
            _msg_count++;
            //ui->labelCount->setText( QString("Messages received: %1").arg(_msg_count) );

            const char* buffer = reinterpret_cast<const char*>(msg.data());
            //qDebug() << msg.size();
            const uint32_t header_size = flatbuffers::ReadScalar<uint32_t>( buffer );
            const uint32_t num_transitions = flatbuffers::ReadScalar<uint32_t>( &buffer[4+header_size] );
            
            std::vector<std::pair<int, NodeStatus>> node_status;
            // check uid in the index, if failed load tree from server
            try{
                for(size_t offset = 4; offset < header_size +4; offset +=3 )
                {
                    const uint16_t uid = flatbuffers::ReadScalar<uint16_t>(&buffer[offset]);
                    _uid_to_index.at(uid);
                }
                
                for(size_t t=0; t < num_transitions; t++)
                {
                    size_t offset = 8 + header_size + 12*t;
                    const uint16_t uid = flatbuffers::ReadScalar<uint16_t>(&buffer[offset+8]);
                    _uid_to_index.at(uid);
                }

                for(size_t offset = 4; offset < header_size +4; offset +=3 )
                {
                    const uint16_t uid = flatbuffers::ReadScalar<uint16_t>(&buffer[offset]);
                    const uint16_t index = _uid_to_index.at(uid);
                    AbstractTreeNode* node = _loaded_tree.node( index );
                    node->status = convert(flatbuffers::ReadScalar<Serialization::NodeStatus>(&buffer[offset+2] ));
                }

                //qDebug() << "--------";
                for(size_t t=0; t < num_transitions; t++)
                {
                    size_t offset = 8 + header_size + 12*t;
                    const uint16_t uid = flatbuffers::ReadScalar<uint16_t>(&buffer[offset+8]);
                    const uint16_t index = _uid_to_index.at(uid);
                    NodeStatus status  = convert(flatbuffers::ReadScalar<Serialization::NodeStatus>(&buffer[offset+11] ));

                    _loaded_tree.node(index)->status = status;
                    node_status.push_back( {index, status} );

                }
            }
            catch( std::out_of_range& err) {
                qDebug() << "Reload tree from server";
                if( !getTreeFromServer() ) {
                    _connected = false;
                    ui->lineEdit->setDisabled(false);
                    _timer->stop();
                    connectionUpdate(false);
                    return;
                }
            }

            // update the graphic part
            emit changeNodeStyle( "BehaviorTree", node_status );
        }
    }
    catch( zmq::error_t& err)
    {
        qDebug() << "ZMQ receive failed: " << err.what();
    }
}

bool SidepanelMonitor::getTreeFromServer()
{
    try{
        zmq::message_t request(0);
        zmq::message_t reply;

        zmq::socket_t  zmq_client( _zmq_context, ZMQ_REQ );
        zmq_client.connect( _connection_address_req.c_str() );

        int timeout_ms = 1000;
        zmq_client.setsockopt(ZMQ_RCVTIMEO,&timeout_ms, sizeof(int) );

        zmq_client.send(request);

        bool received = zmq_client.recv(&reply);
        if( ! received )
        {
            return false;
        }

        const char* buffer = reinterpret_cast<const char*>(reply.data());
        auto fb_behavior_tree = Serialization::GetBehaviorTree( buffer );

        auto res_pair = BuildTreeFromFlatbuffers( fb_behavior_tree );

        _loaded_tree  = std::move( res_pair.first );
        _uid_to_index = std::move( res_pair.second );

        // add new models to registry
        for(const auto& tree_node: _loaded_tree.nodes())
        {
            const auto& registration_ID = tree_node.model.registration_ID;
            if( BuiltinNodeModels().count(registration_ID) == 0)
            {
                addNewModel( tree_node.model );
            }
        }

        try {
            loadBehaviorTree( _loaded_tree, "BehaviorTree" );
        }
        catch (std::exception& err) {
            QMessageBox messageBox;
            messageBox.critical(this,"Error Connecting to remote server", err.what() );
            messageBox.show();
            return false;
        }

        std::vector<std::pair<int, NodeStatus>> node_status;
        node_status.reserve(_loaded_tree.nodesCount());

        //  qDebug() << "--------";

        for(size_t t=0; t < _loaded_tree.nodesCount(); t++)
        {
            node_status.push_back( { t, _loaded_tree.nodes()[t].status } );
        }
        emit changeNodeStyle( "BehaviorTree", node_status );
    }
    catch( zmq::error_t& err)
    {
        qDebug() << "ZMQ client receive failed: " << err.what();
        return false;
    }
    return true;
}

bool SidepanelMonitor::tryConnect() {
    if (_connected) {
        return true;
    }
       

    QString address = (!ui->lineEdit->text().isEmpty()) ? ui->lineEdit->text() : ui->lineEdit->placeholderText();
    QString publisherPort = (!ui->lineEdit_publisher->text().isEmpty()) ? ui->lineEdit_publisher->text() : ui->lineEdit_publisher->placeholderText();
    QString serverPort = (!ui->lineEdit_server->text().isEmpty()) ? ui->lineEdit_server->text() : ui->lineEdit_server->placeholderText();

    if (address.isEmpty() || serverPort.isEmpty() || publisherPort.isEmpty()) {
        return false;
    }

    _connection_address_pub = "tcp://" + address.toStdString() + ":" + publisherPort.toStdString();
    _connection_address_req = "tcp://" + address.toStdString() + ":" + serverPort.toStdString();

    try{
        _zmq_subscriber.connect( _connection_address_pub.c_str() );
        int timeout_ms = 1;
        _zmq_subscriber.setsockopt(ZMQ_SUBSCRIBE, "", 0);
        _zmq_subscriber.setsockopt(ZMQ_RCVTIMEO,&timeout_ms, sizeof(int) );
        _connected = true;
        return true;
    }catch(zmq::error_t& err){
        _connected = false;
        return false;
    }
}

bool SidepanelMonitor::isConnected() {
    return _connected;
}


void SidepanelMonitor::on_Connect()
{
    if (tryConnect() && getTreeFromServer()){
        ui->lineEdit->setDisabled(true);
        ui->lineEdit_publisher->setDisabled(true);
        _timer->start();
        connectionUpdate(true);
    }
    else {
        QMessageBox::warning(this,
            tr("ZeroMQ connection"),
            tr("Was not able to connect to [%1]\n").arg(_connection_address_pub.c_str()),
            QMessageBox::Close);
        ui->lineEdit->setDisabled(false);
        ui->lineEdit_publisher->setDisabled(false);
        _timer->stop();

        connectionUpdate(false);
    }
}
