/********************************************************************************
** Form generated from reading ui file 'mainwindow.ui'
**
** Created: Mon May 11 14:33:51 2009
**      by: Qt User Interface Compiler version 4.4.3
**
** WARNING! All changes made in this file will be lost when recompiling ui file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QHBoxLayout>
#include <QtGui/QMainWindow>
#include <QtGui/QMenuBar>
#include <QtGui/QPushButton>
#include <QtGui/QSplitter>
#include <QtGui/QStatusBar>
#include <QtGui/QTabWidget>
#include <QtGui/QTableView>
#include <QtGui/QTextBrowser>
#include <QtGui/QToolBar>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindowClass
{
public:
    QWidget *centralWidget;
    QVBoxLayout *verticalLayout;
    QSplitter *splitter;
    QTableView *tableTorrents;
    QTabWidget *tabWidget;
    QWidget *tabMain;
    QTableView *tablePeers;
    QWidget *tab2;
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout_2;
    QTextBrowser *textBrowserLog;
    QPushButton *btnRefreshLog;
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindowClass)
    {
    if (MainWindowClass->objectName().isEmpty())
        MainWindowClass->setObjectName(QString::fromUtf8("MainWindowClass"));
    MainWindowClass->resize(571, 408);
    centralWidget = new QWidget(MainWindowClass);
    centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
    verticalLayout = new QVBoxLayout(centralWidget);
    verticalLayout->setSpacing(6);
    verticalLayout->setMargin(11);
    verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
    splitter = new QSplitter(centralWidget);
    splitter->setObjectName(QString::fromUtf8("splitter"));
    splitter->setOrientation(Qt::Vertical);
    tableTorrents = new QTableView(splitter);
    tableTorrents->setObjectName(QString::fromUtf8("tableTorrents"));
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(tableTorrents->sizePolicy().hasHeightForWidth());
    tableTorrents->setSizePolicy(sizePolicy);
    tableTorrents->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableTorrents->setSelectionMode(QAbstractItemView::SingleSelection);
    tableTorrents->setSelectionBehavior(QAbstractItemView::SelectRows);
    splitter->addWidget(tableTorrents);
    tabWidget = new QTabWidget(splitter);
    tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
    sizePolicy.setHeightForWidth(tabWidget->sizePolicy().hasHeightForWidth());
    tabWidget->setSizePolicy(sizePolicy);
    tabWidget->setMinimumSize(QSize(483, 200));
    tabMain = new QWidget();
    tabMain->setObjectName(QString::fromUtf8("tabMain"));
    tablePeers = new QTableView(tabMain);
    tablePeers->setObjectName(QString::fromUtf8("tablePeers"));
    tablePeers->setGeometry(QRect(10, 10, 441, 121));
    tabWidget->addTab(tabMain, QString());
    tab2 = new QWidget();
    tab2->setObjectName(QString::fromUtf8("tab2"));
    horizontalLayout_3 = new QHBoxLayout(tab2);
    horizontalLayout_3->setSpacing(6);
    horizontalLayout_3->setMargin(11);
    horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
    verticalLayout_2 = new QVBoxLayout();
    verticalLayout_2->setSpacing(6);
    verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
    textBrowserLog = new QTextBrowser(tab2);
    textBrowserLog->setObjectName(QString::fromUtf8("textBrowserLog"));

    verticalLayout_2->addWidget(textBrowserLog);

    btnRefreshLog = new QPushButton(tab2);
    btnRefreshLog->setObjectName(QString::fromUtf8("btnRefreshLog"));
    QSizePolicy sizePolicy1(QSizePolicy::Fixed, QSizePolicy::Fixed);
    sizePolicy1.setHorizontalStretch(0);
    sizePolicy1.setVerticalStretch(0);
    sizePolicy1.setHeightForWidth(btnRefreshLog->sizePolicy().hasHeightForWidth());
    btnRefreshLog->setSizePolicy(sizePolicy1);

    verticalLayout_2->addWidget(btnRefreshLog);


    horizontalLayout_3->addLayout(verticalLayout_2);

    tabWidget->addTab(tab2, QString());
    splitter->addWidget(tabWidget);

    verticalLayout->addWidget(splitter);

    MainWindowClass->setCentralWidget(centralWidget);
    menuBar = new QMenuBar(MainWindowClass);
    menuBar->setObjectName(QString::fromUtf8("menuBar"));
    menuBar->setGeometry(QRect(0, 0, 571, 23));
    MainWindowClass->setMenuBar(menuBar);
    mainToolBar = new QToolBar(MainWindowClass);
    mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
    MainWindowClass->addToolBar(Qt::TopToolBarArea, mainToolBar);
    statusBar = new QStatusBar(MainWindowClass);
    statusBar->setObjectName(QString::fromUtf8("statusBar"));
    MainWindowClass->setStatusBar(statusBar);

    retranslateUi(MainWindowClass);

    tabWidget->setCurrentIndex(0);


    QMetaObject::connectSlotsByName(MainWindowClass);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindowClass)
    {
    MainWindowClass->setWindowTitle(QApplication::translate("MainWindowClass", "MainWindow", 0, QApplication::UnicodeUTF8));
    tabWidget->setTabText(tabWidget->indexOf(tabMain), QApplication::translate("MainWindowClass", "Info", 0, QApplication::UnicodeUTF8));
    textBrowserLog->setHtml(QApplication::translate("MainWindowClass", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'Sans Serif'; font-size:9pt; font-weight:400; font-style:normal;\">\n"
"<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"></p></body></html>", 0, QApplication::UnicodeUTF8));
    btnRefreshLog->setText(QApplication::translate("MainWindowClass", "Refresh log", 0, QApplication::UnicodeUTF8));
    tabWidget->setTabText(tabWidget->indexOf(tab2), QApplication::translate("MainWindowClass", "Log", 0, QApplication::UnicodeUTF8));
    Q_UNUSED(MainWindowClass);
    } // retranslateUi

};

namespace Ui {
    class MainWindowClass: public Ui_MainWindowClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
