/*
  Copyright (c) 2018 Sergio Martins <iamsergio@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

  As a special exception, permission is given to link this program
  with any edition of Qt, and distribute the resulting executable,
  without including the source code for Qt in the source distribution.
*/

#include <QLabel>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QThread>
#include <QScreen>

#include <thread>


class KeySender : public QObject
{
    Q_OBJECT
public:
    KeySender(const QString &filename)
        : QObject()
        , m_filename(filename)
    {
    }

    void send_char(QString c)
    {
        if (c == " ") {
            c = R"("\[space]")";
        }

        system(QString("xvkbd -xsendevent -text %1").arg(c).toLatin1());
    }

    void send_line(QString line)
    {
        if (line.trimmed().isEmpty() || line.startsWith("##"))
            return;

        QStringList splitted = line.split(" ");

        if (line.startsWith("#interval")) {
            m_key_interval_ms = splitted[1].toInt();
            m_line_interval_ms = splitted[2].toInt();
            qDebug() << "Setted interval to " << m_key_interval_ms << m_line_interval_ms;
            return;
        } else if (line.startsWith("#sleep")) {
            std::this_thread::sleep_for(std::chrono::milliseconds(splitted[1].toInt()));
            return;
        } else if (line.startsWith("#popup")) {
            line.remove("#popup");
            line = line.trimmed();
            line.replace("\\n", "\n");
            if (!line.isEmpty())
                emit popupTextChange(line);
            return;
        } else if (line.startsWith("#resize_popup")) {
            emit popupSizeChange(splitted[1].toInt(), splitted[2].toInt());
            return;
        }

        for (QChar c : line) {
            send_char(c);
            std::this_thread::sleep_for(std::chrono::milliseconds(m_key_interval_ms));
        }
        send_char(R"("\r")");
        std::this_thread::sleep_for(std::chrono::milliseconds(m_line_interval_ms));
    }

public Q_SLOTS:
    bool process_file()
    {
        qDebug() << "Processing script!";
        QFile file(m_filename);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open " << m_filename;
            return false;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine();

            if (line.startsWith("#quit")) {
                qDebug() << "Quitting";
                qApp->quit();
                return true;
            } else if (line.startsWith("#pause_forever")) {
                qDebug() << "Pausing forever";
                return true;
            }

            send_line(line);
        }

        file.close();
        emit scriptEnded();
        return true;
    }

signals:
    void popupTextChange(const QString &text);
    void popupSizeChange(int width, int height);
    void scriptEnded();

private:

    int m_key_interval_ms = 40;
    int m_line_interval_ms = 100;
    const QString m_filename;
};



int main(int argv, char**argc)
{
    QApplication app(argv, argc);

    if (app.arguments().size() != 2) {
        qWarning() << QString("Usage: %1 <filename>").arg(app.arguments().constFirst());
        return -1;
    }

    auto w = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    auto groupBox = new QGroupBox(w);
    auto layout = new QVBoxLayout(w);
    layout->addWidget(groupBox);
    auto layout2 = new QVBoxLayout(groupBox);
    auto label = new QLabel(w);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignTop | Qt::AlignJustify /*Qt::AlignLeft*/);
    layout2->addWidget(label);
    layout->setMargin(15);
    layout2->setMargin(15);
    w->show();

    auto keySender = new KeySender(app.arguments().at(1));
    QObject::connect(keySender, &KeySender::popupTextChange, label, &QLabel::setText);
    QObject::connect(keySender, &KeySender::popupSizeChange, w, [w] (int width, int height) {
        w->setFixedSize(width, height);
        QScreen *screen = qApp->primaryScreen();
        w->move(screen->geometry().bottomRight() - QPoint(w->width(), w->height()));
    });

    QObject::connect(keySender, &KeySender::scriptEnded, qApp, &QApplication::quit);

    QThread t;
    keySender->moveToThread(&t);
    t.connect(&t, &QThread::started, keySender, &KeySender::process_file);
    t.start();


    return app.exec();
}

#include "main.moc"
