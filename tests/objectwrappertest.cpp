/*
 *  qmlsupporttest.cpp
 *
 *  This file is part of GammaRay, the Qt application inspection and
 *  manipulation tool.
 *
 *  Copyright (C) 2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
 *  Author: Anton Kreuzkamp <anton.kreuzkamp@kdab.com>
 *
 *  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
 *  accordance with GammaRay Commercial License Agreement provided with the Software.
 *
 *  Contact info@kdab.com if any conditions of this licensing are not clear to you.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "baseprobetest.h"

#include <core/objectwrapper.h>

#include <QDebug>
#include <QtTest/qtest.h>
#include <QObject>
#include <QThread>
#include <QSignalSpy>


#include <QObject>
#include <QTimer>
#include <QVector>

#include <type_traits>
#include <memory>

using namespace GammaRay;


class Test : public QObject {
    Q_OBJECT
    Q_PROPERTY(int x READ xx WRITE setXX NOTIFY xxChanged)
    Q_PROPERTY(int y READ yy WRITE setYY NOTIFY yyChanged)
    Q_PROPERTY(QTimer *t MEMBER m_t)

signals:
    void xxChanged();
    void yyChanged();

public:
    Test(Test *parent = nullptr) : QObject(parent), m_t(new QTimer(this)), m_parent(parent) {}
    explicit Test(int x, int y, Test *parent = nullptr) : QObject(parent), m_t(new QTimer(this)), m_xx(x), m_yy(y), m_parent(parent) {}
    virtual ~Test() {}
    int xx() { return m_xx; }
    int yy() { return m_yy; }

    Test *parent() const { return m_parent; }

    void setXX(int x) { m_xx = x; emit xxChanged(); }
    void setYY(int y) { m_yy = y; emit yyChanged(); }

    QString str() { return QStringLiteral("Hello World"); }
    QString echo(const QString &s) { return s; }

    QTimer *m_t = nullptr;

    QVector<Test*> children() const
    {
        return m_children;
    }

    QVector<Test*> m_children;

private:
    int m_xx = 8;
    int m_yy = 10;
    Test *m_parent = nullptr;
};


class LinkedList : public QObject {
    Q_OBJECT

public:
    LinkedList() = default;
    LinkedList(int i) : m_i(i) {}
    LinkedList(int i, LinkedList *next) : m_i(i), m_next(next) { next->m_prev = this; }
    ~LinkedList() { if(m_next) delete m_next; }

    int i() const { return m_i; }
    LinkedList *next() const { return m_next; }
    LinkedList *prev() const { return m_prev; }

private:
    int m_i;
    LinkedList *m_next = nullptr;
    LinkedList *m_prev = nullptr;
};


// class Worker : public QRunnable
// {
// public:
//     void run() override
//     {
//         newThreadObj.reset(new Test());
//         newThreadObj->setObjectName("newThreadObj");
// //         QObject::connect(newThreadObj.get(), &QObject::destroyed, mainThreadObj.get(), &QObject::deleteLater, Qt::DirectConnection);
//     }
//     std::unique_ptr<Test> newThreadObj;
//     std::unique_ptr<QObject> mainThreadObj;
// };

namespace GammaRay {
DECLARE_OBJECT_WRAPPER(QTimer, QPROP_MEMBER(active, isActive()))
DECLARE_OBJECT_WRAPPER(Test,
                       QPROP_MEMBER(x, xx())
                       MEMBER_WITH_NOTIFY(y, yy(), yyChanged)
                       MEMBER(str, str())
                       MEMBER(halloDu, echo("Hello, you."))
                       MEMBER(t, m_t)
                       MEMBER(children, children())
                       MEMBER(parent, parent()) // TODO: Create separated test case for this. Where is the incomplete type actually an issue?
)
DECLARE_OBJECT_WRAPPER(LinkedList,
                       MEMBER(i, i())
                       MEMBER(prev, prev())
                       MEMBER(next, next()) // TODO: Create separated test case for this. Where is the incomplete type actually an issue?
)

class ObjectWrapperTest : public BaseProbeTest
{
    Q_OBJECT
private slots:
    virtual void init()
    {
        createProbe();
    }

    void initTestCase()
    {
//         ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.clear(); // FIXME this should not be necessary!
    }

    void testCleanup()
    {
        QCOMPARE(ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.size(), 0);
        {
            Test t;
            t.m_children = QVector<Test *> { new Test {1, 2, &t}, new Test {3, 4, &t} };
            ObjectHandle<Test> w { &t };
        }
        QCOMPARE(ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.size(), 0);
    }

    void testBasics()
    {
        Test t;
        t.m_children = QVector<Test *> { new Test {1, 2, &t}, new Test {3, 4, &t} };
        ObjectHandle<Test> w { &t };
        //     error<decltype(w->children())>();

        QCOMPARE(w->x(), t.xx());
        QCOMPARE(w->y(), t.yy());
        QCOMPARE(w->str(), t.str());
        QCOMPARE(w->halloDu(), QStringLiteral("Hello, you."));
        QCOMPARE(w->t()->active(), t.m_t->isActive());

        t.setXX(16);
        t.setYY(20);

        QCOMPARE(w->x(), t.xx());
        QCOMPARE(w->y(), t.yy());
        QCOMPARE(w->str(), t.str());
        QCOMPARE(w->halloDu(), QStringLiteral("Hello, you."));
        QCOMPARE(w->t()->active(), t.m_t->isActive());

        static_assert(std::is_same<typename decltype(w->children())::value_type, WeakObjectHandle<Test>>::value,
                      "Something broke with the wrapping of object lists into lists of ObjectHandles...");

        for (auto x : w->children()) {
            QCOMPARE(x->parent()->m_control, w->m_control);
            QCOMPARE(x->x(), x.object()->xx());
            QCOMPARE(x->y(), x.object()->yy());
            QCOMPARE(x->str(), x.object()->str());
            QCOMPARE(x->halloDu(), QStringLiteral("Hello, you."));
            QCOMPARE(x->t()->active(), x.object()->m_t->isActive());
        }
    }

    void testThreadBoundaries()
    {
//         auto task = std::unique_ptr<Worker>(new Worker());
//         task->setAutoDelete(false);
//         QThreadPool::globalInstance()->start(task.get());

        ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.clear(); // FIXME this should not be necessary!
        QThread workerThread;
        workerThread.start();
        Test t;
        t.m_children = QVector<Test *> { new Test {1, 2, &t}, new Test {3, 4, &t} };
        t.moveToThread(&workerThread);

        ObjectHandle<Test> w { &t };
        //     error<decltype(w->children())>();

        QCOMPARE(w->x(), t.xx());
        QCOMPARE(w->y(), t.yy());
        QCOMPARE(w->str(), t.str());
        QCOMPARE(w->halloDu(), QStringLiteral("Hello, you."));
        QCOMPARE(w->t()->active(), t.m_t->isActive());

        t.setXX(16);
        t.setYY(20);

        QCOMPARE(w->x(), t.xx());
        QCOMPARE(w->y(), t.yy());
        QCOMPARE(w->str(), t.str());
        QCOMPARE(w->halloDu(), QStringLiteral("Hello, you."));
        QCOMPARE(w->t()->active(), t.m_t->isActive());

        static_assert(std::is_same<typename decltype(w->children())::value_type, WeakObjectHandle<Test>>::value,
                      "Something broke with the wrapping of object lists into lists of ObjectHandles...");

        for (auto x : w->children()) {
            QCOMPARE(x->parent()->m_control, w->m_control);
            QCOMPARE(x->x(), x.object()->xx());
            QCOMPARE(x->y(), x.object()->yy());
            QCOMPARE(x->str(), x.object()->str());
            QCOMPARE(x->halloDu(), QStringLiteral("Hello, you."));
            QCOMPARE(x->t()->active(), x.object()->m_t->isActive());
        }

        workerThread.quit();
        workerThread.wait();
    }

    void testSelfReference()
    {
        ObjectShadowDataRepository::instance()->m_objectToWrapperControlBlockMap.clear(); // FIXME this should not be necessary!
        LinkedList ll { 5, new LinkedList(6) };
        ObjectHandle<LinkedList> l { &ll };

        QVERIFY(l->object);
        QVERIFY(l->m_control);
        QVERIFY(ll.next()->prev());
        QVERIFY(l->next()->m_control != l->m_control);
        QVERIFY(l->next()->object);
        QVERIFY(l->next()->prev()->object);
        QCOMPARE(l->next()->prev()->m_control, l->m_control);
        QCOMPARE(l->next()->i(), ll.next()->i());
        QCOMPARE(l->next()->prev()->i(), ll.i());
        QCOMPARE(l->next()->prev()->next()->i(), ll.next()->i());
        QCOMPARE(l->next()->prev()->next()->prev()->i(), ll.i());
        QCOMPARE(l->next()->prev()->next()->prev()->next()->i(), ll.next()->i());
    }

    void testMoveHandle()
    {
        Test t;
        auto wptr = new ObjectHandle<Test> { &t };
        ObjectHandle<Test> w = *wptr; // copy
        delete wptr;

        QCOMPARE(w->x(), t.xx());
        QCOMPARE(w->y(), t.yy());
        QCOMPARE(w->str(), t.str());
        QCOMPARE(w->halloDu(), QStringLiteral("Hello, you."));
        QCOMPARE(w->t()->active(), t.m_t->isActive());

        t.setXX(16);
        t.setYY(20);

        QCOMPARE(w->x(), t.xx());
        QCOMPARE(w->y(), t.yy());
        QCOMPARE(w->str(), t.str());
        QCOMPARE(w->halloDu(), QStringLiteral("Hello, you."));
        QCOMPARE(w->t()->active(), t.m_t->isActive());
    }
};

}

QTEST_MAIN(GammaRay::ObjectWrapperTest)

#include "objectwrappertest.moc"
