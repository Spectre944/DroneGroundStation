#include "mapcore.h"
mapcore m_core(30.5842, 50.4234, 8);
cdirprovider m_prov("D:\\tile\\");

mapdraw m_draw(&m_core,&m_prov,0);

double mapcore::getdistance(double lat1, double lon1, double lat2, double lon2)
{
    lat1 = deg2rad(lat1);
    lon1 = deg2rad(lon1);
    lat2 = deg2rad(lat2);
    lon2 = deg2rad(lon2);
    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1) * cos(lat2) *
                   sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}
double mapcore::getdistance(smappoint p1,smappoint p2)
{
    return getdistance(p1.latt, p1.longt, p2.latt, p2.longt);
}

void mapcore::map_xy2longlatt(u_long x,u_long y,double &longt,double &latt)
{
    double t_x=x;
    double t_y=y;
    longt=360 * (t_x / (256 * (1 << zoom))) - 180;
    double term = 0.5 - t_y / (256 * (1<<zoom));
    double exponent = term * 4 * M_PI;
    double numerator = exp(exponent) - 1;
    double denominator = exp(exponent) + 1;
    latt = asin(numerator / denominator)/M_PI*180;
}
void mapcore::screen_xy2longlatt(u_long x,u_long y,double &longt,double &latt)
{
    double t_x=x+offs_map.x;
    double t_y=y+offs_map.y;
    longt=360 * (t_x / (256 * (1 << zoom))) - 180;
    double term = 0.5 - t_y / (256 * (1<<zoom));
    double exponent = term * 4 * M_PI;
    double numerator = exp(exponent) - 1;
    double denominator = exp(exponent) + 1;
    latt = asin(numerator / denominator)/M_PI*180;
}

void mapcore::longlatt2map_xy(u_long &x,u_long &y,smappoint coord)
{
    double sinLatitude = sin(coord.latt * M_PI/180);
    x = ((coord.longt + 180) / 360) * 256 * (1<<zoom);
    y = (0.5 - log((1.0 + sinLatitude) / (1.0 - sinLatitude)) / (4.0 * M_PI)) * 256 * (1<<zoom);
}

void mapcore::longlatt2screen_xy(u_long &x,u_long &y,smappoint coord)
{
    longlatt2map_xy(x,y,coord);
    x-=offs_map.x;
    y-=offs_map.y;
}

void mapcore::gettiles(spoint&how)
{
    longlatt2map_xy(offs_map.x,offs_map.y,m_view);
    offs_map.x=offs_map.x-(screen_size.x>>1);
    offs_map.y=offs_map.y-(screen_size.y>>1);
    how.x=(screen_size.x>>8)+1;
    how.y=(screen_size.y>>8)+1;
}

void mapcore::setcreensize(int x,int y)
{
    screen_size.x=x;
    screen_size.y=y;
}


void mapcore::scroll_h(int how)
{
    u_long x, y;
    longlatt2map_xy(x, y, m_view);
    x += how;

    // Ограничение по X: не выходим за пределы карты [0 .. 256*(1<<zoom))
    u_long maxX = (u_long)256 << zoom;
    if (x < 0)    x = 0;
    if (x >= maxX) x = maxX - 1;

    map_xy2longlatt(x, y, m_view.longt, m_view.latt);
    clampview();
}

void mapcore::scroll_v(int how)
{
    u_long x, y;
    longlatt2map_xy(x, y, m_view);
    y += how;

    // Ограничение по Y: не выходим за пределы карты [0 .. 256*(1<<zoom))
    u_long maxY = (u_long)256 << zoom;
    if (y < 0)    y = 0;
    if (y >= maxY) y = maxY - 1;

    map_xy2longlatt(x, y, m_view.longt, m_view.latt);
    clampview();
}

void mapdraw::draw(QPainter &p)
{
    if(!p_view)
        return;
    p_core->setcreensize(p_view->contentsRect().width(),p_view->contentsRect().height());
    spoint how;
    p_core->gettiles(how);
    int bx=(p_core->offs_map.x>>8);
    int by=(p_core->offs_map.y>>8);
    int zoom = p_core->getzoom();
    QImage image;
    for(int i=0;i<how.x+1;i++)
    {
        for(int j=0;j<how.y+1;j++)
        {
            if(p_prov->gettile(zoom,bx+i,by+j,image))
                p.drawImage(i*256,j*256,image);
        }
    }
}
