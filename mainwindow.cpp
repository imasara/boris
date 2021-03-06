/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/***************************************************************************
 *            mainwindow.cpp
 *
 *  Tue Nov 26 16:56:00 CEST 2013
 *  Copyright 2013 Lars Muldjord
 *  muldjordlars@gmail.com
 ****************************************************************************/

/*
 *  This file is part of Boris.
 *
 *  Boris is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Boris is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Boris; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include "mainwindow.h"
#include "about.h"
#include "loader.h"

#include <stdio.h>
#include <math.h>
#include <QApplication>
#include <QTime>
#include <QSettings>
#include <QDir>
#include <QTextStream>
#include <QDesktopWidget>

//#define DEBUG

extern QSettings *settings;

MainWindow::MainWindow()
{
  QTime time = QTime::currentTime();
  qsrand((uint)time.msec());

  if(!settings->contains("boris_x")) {
    settings->setValue("boris_x", QApplication::desktop()->width() / 2);
  }
  if(!settings->contains("boris_y")) {
    settings->setValue("boris_y", QApplication::desktop()->height() / 2);
  }
  if(!settings->contains("show_welcome")) {
    settings->setValue("show_welcome", "true");
  }
  if(!settings->contains("behavs_path")) {
    settings->setValue("behavs_path", "data/behavs");
  }
  if(!settings->contains("clones")) {
    settings->setValue("clones", "2");
  }
  if(!settings->contains("size")) {
    settings->setValue("size", "64");
  }
  if(!settings->contains("independence")) {
    settings->setValue("independence", "70");
  }
  if(!settings->contains("stats")) {
    settings->setValue("stats", "never");
  } else if(settings->value("stats") == "true") {
    settings->setValue("stats", "always");
  } else if(settings->value("stats") == "false") {
    settings->setValue("stats", "never");
  }
  if(!settings->contains("chatter")) {
    settings->setValue("chatter", "true");
  }
  if(!settings->contains("sound")) {
    settings->setValue("sound", "true");
  }
  if(!settings->contains("volume")) {
    settings->setValue("volume", "50");
  }
  if(!settings->contains("lemmy_mode")) {
    settings->setValue("lemmy_mode", "false");
  }
  if(!settings->contains("time_factor")) {
    settings->setValue("time_factor", "1");
  }
  if(!settings->contains("weather_path")) {
    settings->setValue("weather_path", "data/weathers");
  }
  if(!settings->contains("feed_url")) {
    settings->setValue("feed_url", "http://rss.slashdot.org/Slashdot/slashdotMain");
  }
  if(!settings->contains("weather_city")) {
    settings->setValue("weather_city", "Copenhagen");
  }
  if(!settings->contains("weather_key")) {
    settings->setValue("weather_key", "fe9fe6cf47c03d2640d5063fbfa053a2");
  }

  if(settings->value("show_welcome", "true").toBool()) {
    About about(this);
    about.exec();
  }
  
  behaviours = new QList<Behaviour>;
  weathers = new QList<Behaviour>;
  weather = new Weather;
  chatLines = new QList<ChatLine>;
  
  if(Loader::loadBehaviours(settings->value("behavs_path", "data/behavs").toString(), behaviours)) {
    qInfo("Behaviours loaded ok... :)\n");
  } else {
    qInfo("Error when loading some behaviours, please check your png and dat files\n");
  }
  
  if(Loader::loadBehaviours(settings->value("weather_path", "data/weather").toString(), weathers)) {
    qInfo("Weather types loaded ok... :)\n");
  } else {
    qInfo("Error when loading some weather types, please check your png and dat files\n");
  }

  createActions();
  createTrayIcon();
  trayIcon->show();

  netComm = new NetComm();
  connect(netComm, &NetComm::weatherUpdated, this, &MainWindow::updateWeather);
  connect(netComm, &NetComm::feedUpdated, this, &MainWindow::updateChatLines);

  clones = settings->value("clones", "4").toInt();
  qInfo("Spawning %d clone(s)\n", clones);
  addBoris(settings->value("clones", "4").toInt());

  collisTimer.setInterval(1000);
  collisTimer.setSingleShot(true);
  connect(&collisTimer, &QTimer::timeout, this, &MainWindow::checkCollisions);
  collisTimer.start();
}

MainWindow::~MainWindow()
{
  delete trayIcon;
  // Stop all sounds before exiting, to avoid Pulseaudio crash on Linux
  for(int a = 0; a < behaviours->length(); ++a) {
    for(int b = 0; b < behaviours->at(a).behaviour.length(); ++b) {
      if(behaviours->at(a).behaviour.at(b).soundFx != NULL &&
         behaviours->at(a).behaviour.at(b).soundFx->isPlaying()) {
        behaviours->at(a).behaviour.at(b).soundFx->stop();
      }
    }
  }
  delete behaviours;
  delete weathers;
  delete chatLines;
}

void MainWindow::addBoris(int clones)
{
  for(int a = 0; a < clones; ++a) {
    borises.append(new Boris(behaviours, weathers, weather, chatLines, this));
    connect(earthquakeAction, &QAction::triggered, borises.last(), &Boris::earthquake);
    connect(teleportAction, &QAction::triggered, borises.last(), &Boris::teleport);
    connect(weatherAction, &QAction::triggered, borises.last(), &Boris::triggerWeather);
    borises.last()->show();
    borises.last()->earthquake();
  }
}

void MainWindow::removeBoris(int clones)
{
  // Reset all Boris collide pointers within all existing clones to prevent crash
  for(int a = 0; a < borises.length(); ++a) {
    borises.at(a)->boris = NULL;
  }
  for(int a = 0; a < clones; ++a) {
    delete borises.last();
    borises.removeLast();
  }
}

void MainWindow::createActions()
{
  aboutAction = new QAction(tr("&Config / about..."), this);
  aboutAction->setIcon(QIcon(":icon_about.png"));
  connect(aboutAction, &QAction::triggered, this, &MainWindow::aboutBox);

  earthquakeAction = new QAction(tr("&Earthquake!!!"), this);
  earthquakeAction->setIcon(QIcon(":earthquake.png"));
  teleportAction = new QAction(tr("&Beam me up, Scotty!"), this);
  teleportAction->setIcon(QIcon(":teleport.png"));

  weatherAction = new QAction(tr("Updating weather..."), this);
  
  quitAction = new QAction(tr("&Quit"), this);
  quitAction->setIcon(QIcon(":icon_quit.png"));
  connect(quitAction, &QAction::triggered, this, &MainWindow::killAll);
}

void MainWindow::createTrayIcon()
{
  trayIconMenu = new QMenu(this);
  trayIconMenu->addAction(aboutAction);
  trayIconMenu->addAction(earthquakeAction);
  trayIconMenu->addAction(teleportAction);
  trayIconMenu->addAction(weatherAction);
  trayIconMenu->addSeparator();
  trayIconMenu->addAction(quitAction);

  trayIcon = new QSystemTrayIcon(this);

  QImage iconImage(":icon.png");
  Loader::setClothesColor(iconImage);
  QIcon icon(QPixmap::fromImage(iconImage));
  trayIcon->setToolTip("Boris");
  trayIcon->setIcon(icon);
  trayIcon->setContextMenu(trayIconMenu);
}

void MainWindow::aboutBox()
{
  About about(this);
  about.exec();
  int newSize = settings->value("size", "32").toInt();
  bool soundEnable = settings->value("sound", "true").toBool();
  bool showStats = settings->value("stats") == "always";
  int independence = settings->value("independence", "0").toInt();
  qreal volume = (qreal)settings->value("volume", "100").toInt() / 100.0;
  for(int a = 0; a < borises.length(); ++a) {
    borises.at(a)->updateBoris(newSize, showStats, soundEnable, independence);
    for(int b = 0; b < behaviours->length(); ++b) {
      for(int c = 0; c < behaviours->at(b).behaviour.length(); ++c) {
        if(behaviours->at(b).behaviour.at(c).soundFx != NULL) {
          behaviours->at(b).behaviour.at(c).soundFx->setVolume(volume);
        }
      }
    }
  }
  if(settings->value("clones", "2").toInt() != clones) {
    if(clones > settings->value("clones", "2").toInt()) {
      removeBoris(clones - settings->value("clones", "2").toInt());
    } else if(clones < settings->value("clones", "2").toInt()) {
      addBoris(settings->value("clones", "2").toInt() - clones);
    }
    clones = settings->value("clones", "2").toInt();
  }

  netComm->updateAll();
}

void MainWindow::mousePressEvent(QMouseEvent* event)
{
  if(event->button() == Qt::RightButton) {
    trayIconMenu->exec(QCursor::pos());
  }
}

void MainWindow::checkCollisions()
{
  for(int a = 0; a < borises.length(); ++a) {
    for(int b = a + 1; b < borises.length(); ++b) {
      int xA = borises.at(a)->pos().x();
      int yA = borises.at(a)->pos().y();
      int xB = borises.at(b)->pos().x();
      int yB = borises.at(b)->pos().y();
      double hypotenuse = sqrt((yB - yA) * (yB - yA) + (xB - xA) * (xB - xA));
      if(hypotenuse < 128) {
        borises.at(a)->collide(borises.at(b));
        borises.at(b)->collide(borises.at(a));
      }
    }
  }
  collisTimer.start();
}

void MainWindow::killAll()
{
  QTimer::singleShot(2000, qApp, SLOT(quit()));
  for(int a = 0; a < borises.length(); ++a) {
    borises.at(a)->changeBehaviour("casual_wave");
  }
}

void MainWindow::updateWeather()
{
  *weather = netComm->getWeather();
  if(weather->temp != 66.6) {
    weatherAction->setText(QString::number(weather->temp) + tr(" degrees Celsius"));
  } else {
    weatherAction->setText(tr("Couldn't find city"));
  }
  weatherAction->setIcon(QIcon(":" + weather->icon + ".png"));
}


void MainWindow::updateChatLines()
{
  chatLines->clear();
  if(!settings->contains("chat_file")) {
    settings->setValue("chat_file", "data/chatter.dat");
  }

  QFile chatFile(settings->value("chat_file", "data/chatter.dat").toString());
  if(chatFile.open(QIODevice::ReadOnly)) {
    do {
      QStringList snippets = QString(chatFile.readLine()).split(";");
      ChatLine chatLine;
      chatLine.type = snippets.at(0);
      chatLine.text = snippets.at(1).trimmed();
      chatLines->append(chatLine);
    } while(chatFile.canReadLine());
  }
  chatFile.close();

  chatLines->append(netComm->getFeedLines());
}
