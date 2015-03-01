/************************************************************************************
 * EMSLoader - Open Source S12X Serial Monitor S19 Loader                           *
 * Copyright (C) 2013  Michael Carpenter (malcom2073@gmail.com)                     *
 *                                                                                  *
 * This file is a part of EMSLoader                                                 *
 *                                                                                  *
 * EMSLoader is free software; you can redistribute it and/or modify                *
 * it under the terms of the GNU General Public License as published by             *
 * the Free Software Foundation, either version 3 of the License, or                *
 * (at your option) any later version.                                              *
 *                                                                                  *
 * EMSLoader is distributed in the hope that it will be useful,                     *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU                *
 * General Public License for more details.                                         *
 *                                                                                  *
 * You should have received a copy of the GNU General Public License                *
 * along with EMSLoader.  If not, see <http://www.gnu.org/licenses/>.               *
 ************************************************************************************/

#include "mainwindow.h"
#include <QtSerialPort/QSerialPortInfo>
#include <QTimer>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	ui.setupUi(this);
	QTimer *porttimer = new QTimer(this);
	connect(porttimer,SIGNAL(timeout()),this,SLOT(comPortRefresh()));
	porttimer->start(1000);

	connect(ui.loadFirmwarePushButton,SIGNAL(clicked()),this,SLOT(loadButtonClicked()));
	connect(ui.selectFirmwarePushButton,SIGNAL(clicked()),this,SLOT(selectFileButtonClicked()));
	connect(ui.selectSavePushButton,SIGNAL(clicked()),this,SLOT(selectSaveButtonClicked()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::comPortRefresh()
{
	QString currentport = ui.portNameComboBox->currentText();
	int currentindex = -1;
	ui.portNameComboBox->clear();
	QList<QSerialPortInfo> portlist = QSerialPortInfo::availablePorts();
	for (int i=0;i<portlist.size();i++)
	{
		ui.portNameComboBox->addItem(portlist[i].portName(),QVariant(portlist[i].systemLocation()));
		//qDebug() << portlist[i].portName() << portlist[i].systemLocation() << portlist[i].description();
		if (portlist[i].portName() == currentport)
		{
			currentindex = i;
		}
	}
	if (currentindex >= 0)
	{
		ui.portNameComboBox->setCurrentIndex(currentindex);
	}

}
void MainWindow::loadButtonClicked()
{
	if (ui.lineEdit->text() == "")
	{
		QMessageBox::information(0,"Error","Select a file to load first");
		return;
	}
	if (!QFile::exists(ui.lineEdit->text()))
	{
		QMessageBox::information(0,"Error","File selected does not seem to exist. Ensure you have the proper permissions to access that file");
		return;
	}
	m_loaderThread = new LoaderThread();
	connect(m_loaderThread,SIGNAL(progress(quint64,quint64)),this,SLOT(loaderProgress(quint64,quint64)));
	connect(m_loaderThread,SIGNAL(done(quint64)),this,SLOT(loaderDone(quint64)));
	connect(m_loaderThread,SIGNAL(error(QString)),this,SLOT(loaderError(QString)));
	m_loaderThread->startLoad(m_loadedS19,ui.portNameComboBox->itemData(ui.portNameComboBox->currentIndex()).toString());
}
void MainWindow::loadFileDone()
{
	QString labeltext = "S19 Loaded\n";
	labeltext += QString::number(m_loadedS19->getRecordCount()) + " records loaded\n";
	labeltext += QString::number(m_loadedS19->getCompactRecordCount()) + " compacted records to flash\n";
	labeltext += QString::number(m_loadedS19->getTotalSize()) + " total bytes to flash";
	ui.label->setText(labeltext);
}

void MainWindow::selectFileButtonClicked()
{
	QString filename = QFileDialog::getOpenFileName(this,"Open FW File");
	if (filename == "")
	{
		return;
	}
	ui.lineEdit->setText(filename);
	ui.label->setText("Loading S19...");
	//loadButtonClicked();
	m_loadedS19 = new S19File();
	connect(m_loadedS19,SIGNAL(done()),this,SLOT(loadFileDone()));
	m_loadedS19->loadFile(filename);

}
void MainWindow::loaderProgress(quint64 current,quint64 total)
{
	if (ui.progressBar->maximum() != total)
	{
		ui.progressBar->setMaximum(total);
	}
	ui.progressBar->setValue(current);
}

void MainWindow::loaderDone(quint64 msecs)
{
	ui.progressBar->setValue(ui.progressBar->maximum());
	ui.label->setText(ui.label->text() + "\nFlashing complete in: " + QString::number(msecs / 1000.0) + " seconds");
	m_loaderThread->deleteLater();
	m_loaderThread = 0;
	QMessageBox::information(0,"Congratulations!","Flashing is complete! Please remove SM jumper and reset the device");
}
void MainWindow::selectSaveButtonClicked()
{
	QString filename = QFileDialog::getSaveFileName(this,"Save FW File");
	if (filename == "")
	{
		return;
	}
	m_loaderThread = new LoaderThread();
	connect(m_loaderThread,SIGNAL(progress(quint64,quint64)),this,SLOT(loaderProgress(quint64,quint64)));
	connect(m_loaderThread,SIGNAL(done()),this,SLOT(loaderDone()));
	m_loaderThread->startRip(filename,ui.portNameComboBox->itemData(ui.portNameComboBox->currentIndex()).toString());
}
void MainWindow::loaderError(QString error)
{
	QMessageBox::information(0,"Error",error);
}
