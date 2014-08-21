#include "task.h"
#include <stdio.h>
#include <QVector>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QDebug>
#include <cmath> //for round()

Task::Task(QObject *parent) :
    QObject(parent)
  , jpdReady(false)
{
}

void Task::_init(FG::pdfType type, int nStates, int vRange, double extRatio, double gVar)
{
    _type = type;
    _nS = nStates;
    _vR = vRange;
    _extR = extRatio;
    _gV = gVar;

    QVector<int> s, c; //vector of scopes and cardinals
    A.init(FG::Variable, 1, _nS); fA.init(FG::Factor, -1, _nS);
    B.init(FG::Variable, 2, _nS); fB.init(FG::Factor, -2, _nS);
    fAB.init(FG::Factor, -3, _nS);

    //asssign neighborhood
    fA.addNeighbor(A.getID()); A.addNeighbor(fA.getID()); A.addNeighbor(fAB.getID());
    fB.addNeighbor(B.getID()); B.addNeighbor(fB.getID()); B.addNeighbor(fAB.getID());
    fAB.addNeighbor(A.getID()); fAB.addNeighbor(B.getID());

    s.clear(); c.clear(); s << A.getID(); c << A.getCard(); fA.initFactor(s, c);
    s.clear(); c.clear(); s << B.getID(); c << B.getCard(); fB.initFactor(s, c);
    s.clear(); c.clear(); s << A.getID() << B.getID(); c << A.getCard() << B.getCard();
    fAB.initFactor(s,c);

    //training flow
    ufA_to_A.setID(fA.getID()); ufA_to_A.setScope(A.getID()); ufA_to_A.setCard(A.getCard()); ufA_to_A.setSourceID(fA.getID());
    ufB_to_B.setID(fB.getID()); ufB_to_B.setScope(B.getID()); ufB_to_B.setCard(B.getCard()); ufB_to_B.setSourceID(fB.getID());
    uA_to_fAB.setID(A.getID()); uA_to_fAB.setScope(A.getID()); uA_to_fAB.setCard(A.getCard()); uA_to_fAB.setSourceID(A.getID());
    uB_to_fAB.setID(B.getID()); uB_to_fAB.setScope(B.getID()); uB_to_fAB.setCard(B.getCard()); uB_to_fAB.setSourceID(B.getID());

    //inference flow
    ufAB_to_B.setID(fAB.getID()); ufAB_to_B.setScope(B.getID()); ufAB_to_B.setCard(B.getCard()); ufAB_to_B.setSourceID(fAB.getID());
}

bool Task::_trainJPD(QString datasetFname, QString *resjpdFname)
{
    QString JPDfname = datasetFname; JPDfname.append(".jpd"); _datasetName = datasetFname;
    QString JPDinfofname = JPDfname; JPDinfofname.append(".info");
    if(resjpdFname!=NULL) *resjpdFname = JPDfname; //if we need to export the JPDfname to the caller

    bool result = true;
    bool success;
    int done = 0, currentp; //percentage of reading file
    qint64 temp;
    QString line;
    QStringList strList;
    CPMF pmf(_type, _nS, _vR, _extR, _gV);
    CFactor jpd, product, F;
    QVector<double> tmpjpd;
    quint64 fileSize;
    QFile raw(datasetFname);    //the dataset used for building jpd
    QFile jpdResult(JPDfname);  //the resulted jpd will be stored in this file
    QFile jpdInfo(JPDinfofname);

    //get the filesize of the dataset
    fileSize = raw.size();

    //F is a temporary factor
    F = fAB.getFactor();        //get the local function (ie. cpt) of factor fAB
    jpd.setScope(F.getScope());
    jpd.setCard(F.getCard());
    QVector<double> nulljpd(getElementProd(F.getCard()),0.0);
    jpd.setJPD(nulljpd);

    if(!raw.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    printf("Computing jpd via message product...\n");
    printf("Processing file %d%%\n",done);
    while(!raw.atEnd())
    {
        currentp = int(round((double)raw.pos()*100.0/(double)fileSize));
        if(currentp > done){done = currentp; printf("Processing file %d\%\n",done);}

        line = raw.readLine();
        if(!line.isEmpty())
        {
            strList = line.split(',');
            pmf.setVarValue(strList.at(0).toInt());
            fA.setFactor(pmf.getVarStates());
            ufA_to_A.updateFactor(fA.computeMessage());
            A.putMessage(ufA_to_A);
            uA_to_fAB.updateFactor(A.computeMessage(fAB.getID(), &success));
            if(!success) qFatal("Something wrong with message computation from A node to fAB!");

            pmf.setVarValue(strList.at(1).toInt());
            fB.setFactor(pmf.getVarStates());
            ufB_to_B.updateFactor(fB.computeMessage());
            B.putMessage(ufB_to_B);
            uB_to_fAB.updateFactor(B.computeMessage(fAB.getID(), &success));
            if(!success) qFatal("Something wrong with message computation from B node to fAB!");

            /* IMPORTANT: The Order DOES Matter! */
            product = uA_to_fAB;
            product.prod(uB_to_fAB, false);
            if(product.getScope()!=jpd.getScope()) qFatal("Scope mismatch! Something wrong with the factor product operation!");
            tmpjpd = addVectorElements(jpd.getJPD(), product.getJPD());
            jpd.setJPD(tmpjpd);
        }
    }
    printf("\n");
    raw.close();

    qDebug() << QString("Normalize JPD...");
    /* 3rd step: normalize jpd */
    tmpjpd = normalizeStates(jpd.getJPD());
    jpd.setJPD(tmpjpd);

    /* 4th step: conditioning jpd */
    qDebug() << "Conditioning JPD on A to get B...";
    jpd.getCPT(A.getID());
    tmpjpd = jpd.getJPD();

    /* and then write to a file */
    qDebug() << "Write the result to a file...";
    if(!jpdResult.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Cannot open the file for writing!";
        return false;
    }
    QTextStream jpdOut(&jpdResult);
    QTextStream jpdInfoText(&jpdInfo);
    temp = tmpjpd.count();
    for(int i=0; i<(int)temp; i++)
        jpdOut << QString("%1\n").arg(tmpjpd.at(i));
    jpdResult.close();
    //write the info as well
    jpdInfo.open(QIODevice::WriteOnly | QIODevice::Text);
    jpdInfoText << QString("Original dataset: %1\n").arg(datasetFname);
    switch(_type)
    {
    case FG::Single:    jpdInfoText << QString("Using pdf type %1 (Single)\n").arg(_type); break;
    case FG::Gaussian:  jpdInfoText << QString("Using pdf type %1 (Gaussian)\n").arg(_type); break;
    case FG::Binary:    jpdInfoText << QString("Using pdf type %1 (Binary)\n").arg(_type); break;
    case FG::Beta:      jpdInfoText << QString("Using pdf type %1 (Beta)\n").arg(_type); break;
    }

    jpdInfoText << QString("Number of States: %1\n").arg(_nS);
    jpdInfoText << QString("Value range: %1\n").arg(_vR);
    jpdInfoText << QString("Extended ratio: %1\n").arg(_extR);
    jpdInfoText << QString("Gaussian Variance: %1\n").arg(_gV);
    jpdInfo.close();

    fAB.setFactor(tmpjpd);

    //indicate that the jpd is ready for inference
    jpdReady = true;
    return result;
}

bool Task::_loadJPD(QString jpdFname)
{
    QFile jpdFile(jpdFname);
    QString line;
    QVector<double> jpd;
    if(!jpdFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qFatal("Cannot open the jpd file!"); return false;
    }
    while(!jpdFile.atEnd())
    {
        line = jpdFile.readLine();
        if(!line.isEmpty())
        {
            jpd.append(line.toDouble());
        }
    }
    jpdFile.close();
    fAB.setFactor(jpd);
    jpdReady = true;
    return true;
}

void Task::run()
{
    printf("Performing inference...\n");
    qDebug() << QString("Result filename: %1").arg(_resultFileName);
//    emit finished();
//    return;
    QFile fRes(_resultFileName);
    QTextStream fOut(&fRes);
    if(!fRes.open(QIODevice::WriteOnly | QIODevice::Text))
        qFatal("Cannot create file for storing the result!");
    CPMF pmf(_type, _nS, _vR, _extR, _gV);
    bool success;
    int res;
    int valA;
    int done = 0, currentp; //percentage of processing/reading file
    quint64 dataSize;

    if(!_useDataset)
    {
        dataSize = 2*_vR+1;
        double cntr = 0.0;
        for(int i=-_vR; i<=_vR; i++)
        {
            currentp = int(round(cntr*100.0/(double)dataSize));
            if(currentp > done){done = currentp; printf("Processing data point %d\%\n",done);} cntr+=1.0;
            pmf.setVarValue(i);
            fA.setFactor(pmf.getVarStates());
            ufA_to_A.updateFactor(fA.computeMessage());
            A.putMessage(ufA_to_A);
            uA_to_fAB.updateFactor(A.computeMessage(fAB.getID(), &success));
            if(!success) qFatal("Something wrong with message computation from A to fAB!");
            fAB.putMessage(uA_to_fAB);
            ufAB_to_B.updateFactor(fAB.computeMessage(B.getID(), &success));
            if(!success) qFatal("Something wrong with message computation in the fAB node!");
            pmf.setVarStates(normalizeStates(ufAB_to_B.getJPD()));
            res = pmf.getiVarValue();
            fOut << QString("%1,%2\n").arg(i).arg(res);
        }
    }
    else
    {
        QFile fDataset(_datasetName);
        QString line;
        QStringList strList;
        dataSize = fDataset.size();

        if(!fDataset.open(QIODevice::ReadOnly | QIODevice::Text))
            qFatal("Cannot open the dataset for inference!");
        printf("Performing inference with dataset...\n");
        printf("Processing file %d%%\n",done);
        while(!fDataset.atEnd())
        {
            currentp = int(round((double)fDataset.pos()*100.0/(double)dataSize));
            if(currentp > done){done = currentp; printf("Processing file %d\%\n",done);}
            line = fDataset.readLine();
            if(!line.isEmpty())
            {
                strList = line.split(',');
                valA = strList.at(0).toInt();
                pmf.setVarValue(valA);
                fA.setFactor(pmf.getVarStates());
                ufA_to_A.updateFactor(fA.computeMessage());
                A.putMessage(ufA_to_A);
                uA_to_fAB.updateFactor(A.computeMessage(fAB.getID(), &success));
                if(!success) qFatal("Something wrong with message computation from A to fAB!");
                fAB.putMessage(uA_to_fAB);
                ufAB_to_B.updateFactor(fAB.computeMessage(B.getID(), &success));
                if(!success) qFatal("Something wrong with message computation in the fAB node!");
                pmf.setVarStates(normalizeStates(ufAB_to_B.getJPD()));
                res = pmf.getiVarValue();
                fOut << QString("%1,%2\n").arg(valA).arg(res);
            }
        }
        fDataset.close();
    }
    fRes.close();

    printf("Inference is done!\n");

    // And later on:
    emit finished();
}


