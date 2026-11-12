#ifndef COORDINATESMANAGER_H
#define COORDINATESMANAGER_H

#include <QObject>
#include <QQueue>
#include "mapwidget.h"


class TextEditCoordinates;

class CoordinatesManager : public QObject
{
    Q_OBJECT

public:
    static CoordinatesManager* instance();

    void registerCoordinateField(TextEditCoordinates* field);
    void unregisterCoordinateField(TextEditCoordinates* field);
    void requestCoordinatesFromMap(TextEditCoordinates* requester);
    void setMapReference(MapWidget*);

private slots:
    void onCoordinatesArrayReady(QVector<smappoint> coordinates);
    void onDrawingCanceled();
    void onCoordinateFieldDestroyed(QObject* field);

private:
    explicit CoordinatesManager(QObject *parent = nullptr);
    static CoordinatesManager* m_instance;

    MapWidget* m_map = nullptr;
    TextEditCoordinates* m_currentRequester = nullptr;
    QQueue<TextEditCoordinates*> m_requestQueue;

};

#endif // COORDINATESMANAGER_H
