/*
Abstract:
  QT application implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "ui/mainwindow.h"
#include "ui/mainwindow_embedded.h"
#include <apps/base/app.h>
//qt includes
#include <QtGui/QApplication>

inline void InitResources()
{
  Q_INIT_RESOURCE(icons);
}

//external declaration
QPointer<QMainWindow> CreateMainWindow(int arg, char* argv[]);

namespace
{
  class QTApplication : public Application
  {
  public:
    QTApplication()
    {
    }

    virtual int Run(int argc, char* argv[])
    {
      QApplication qapp(argc, argv);
      InitResources();
      //main ui
      QPointer<QMainWindow> win = CreateMainWindow(argc, argv);
      return qapp.exec();
    }
  };
}

std::auto_ptr<Application> Application::Create()
{
  return std::auto_ptr<Application>(new QTApplication());
}
