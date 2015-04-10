APP_NAME = HFR10Service

CONFIG += qt warn_on
QT += network xml

include(config.pri)

LIBS += -lbb -lbbsystem -lbbplatform -lbbpim -lunifieddatasourcec -lbbdata
