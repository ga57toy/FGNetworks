#ifndef TASK_H
#define TASK_H

#include <QObject>
#include "fgcommon.h"
#include "cnode.h"
#include "cpmf.h"

class Task : public QObject
{
    Q_OBJECT
public:
    explicit Task(QObject *parent = 0);
    void _init(FG::pdfType type, int nStates, int vRange, double extRatio, double gVar);
    bool _trainJPD(QString datasetFname, QString *resjpdFname = NULL);
    bool _loadJPD(QString jpdFname);
    void _setResultFile(QString fname) {_resultFileName=fname;}
    void _setInferMode(bool useDataset) {_useDataset=useDataset;}
public slots:
    void run();                     //do the inference
signals:
    void finished();
private:
    /*----------- member variables -------------*/
    QString _datasetName;           //name of dataset file
    QString _resultFileName;        //name of the file for inference result
    int _nS;                        //number of states
    int _vR;                        //value Range
    double _extR;                   //extended Ratio
    double _gV;                     //Gaussian variance
    FG::pdfType _type;              //type of pdf
    CNode A, B, fA, fB, fAB;
    //training flow:
    CMessage ufA_to_A, uA_to_fAB, ufB_to_B, uB_to_fAB;
    //inference flow:
    CMessage ufAB_to_B;             //what is the value of p(B) given A?
    bool jpdReady;
    bool _useDataset;
    /*----------- member functions -------------*/
    void infer();
};

#endif // TASK_H

