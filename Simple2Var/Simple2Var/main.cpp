#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include "task.h"

#include <getopt.h>             //buat getopt_long, jika yang dipakai adalah getopt, maka butuh unistd.h
#include <stdlib.h>             //atoi(), atof()
#include <stdio.h>

int getParams(int c, char **v);
void help(void);

/* network parameters */
QString datasetName;
QString jpdName;
QString resultFileName;
int pdfType = -1;                    //according to fgcommon.h, 0=Single, 1=Gaussian, 2=Binary, 3=Beta
int nStates = 0;
int vRange = 0;
double extRatio = 0.0;
double gVar = 0.0;
bool datasetInference = false;

int main(int argc, char *argv[]) //this main function should return error value, 0 = OK
{
    /* check for program parameters */
    if((getParams(argc, argv) < 0)) {help(); return -1;}

    QCoreApplication a(argc, argv);

    // Task parented to the application so that it will be deleted by the application.
    Task *task = new Task(&a);

    // Initialize the network
    task->_init(static_cast<FG::pdfType>(pdfType), nStates, vRange, extRatio, gVar);

    // train or load jpd?
    if(jpdName.isNull() || jpdName.isEmpty()) //then train jpd
    {
        if(datasetName.isNull() || datasetName.isEmpty())
        {
            printf("Cannot train jpd! Missing dataset file!!!\n"); return -1;
        } else
        {
            if(!task->_trainJPD(datasetName))
            {
                printf("Error during training jpd!\n"); return -1;
            }
            else
            {
                resultFileName = datasetName;
                resultFileName.append(".res");
            }
        }
    }
    else
    {
        if(!task->_loadJPD(jpdName))
        {
            printf("Error during loading the jpd file!\n"); return -1;
        }
        else
        {
            resultFileName = jpdName;
            resultFileName.replace(".jpd",".res");
        }
    }
    task->_setResultFile(resultFileName);   //this filename will be used during run()
    task->_setInferMode(datasetInference);

    /* print parameter information */
    qDebug() << QString("Dataset name: %1").arg(datasetName);
    qDebug() << QString("JPD name: %1").arg(jpdName);
    qDebug() << QString("Result filename: %1").arg(resultFileName);

    // This will cause the application to exit when the task signals finished.
    QObject::connect(task, SIGNAL(finished()), &a, SLOT(quit()));

    // This will run the task from the application event loop.
    QTimer::singleShot(0, task, SLOT(run()));

    return a.exec();

}


int getParams(int c, char **v)
{
    int chr;
    int r = 0; //returned error value, -1 for

    // Karena getopt biasanya dalam loop sampai tidak ditemukan lagi parameter valid
    while (1)
    {
        static struct option long_options[] =
        {
            /* These options don't set a flag. We distinguish them by their indices. */
            {"dataset",  required_argument, 0, 'd'},
            {"jpdfile",  required_argument, 0, 'j'},
            {"nstates",  required_argument, 0, 'n'},
            {"vrange",  required_argument, 0, 'r'},
            {"extratio", required_argument, 0, 'x'},
            {"gvariance", required_argument, 0, 'v'},
            {"pdftype", required_argument, 0, 't'},
            {"confirmdataset", no_argument, 0, 'c'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        chr = getopt_long (c, v, "d:j:n:r:x:v:t:ch", long_options, &option_index);

        /* Detect the end of the options. */
        if (chr == -1) break;

        switch (chr)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0) break;
                printf ("unknown option %s", long_options[option_index].name);
                if (optarg) printf (" with arg %s", optarg);
                printf ("\n");
                break;

            case 'd': datasetName = QString("%1").arg(optarg);break;
            case 'j': jpdName = QString("%1").arg(optarg); break;
            case 'n': nStates = atoi(optarg); break;
            case 'r': vRange = atoi(optarg); break;
            case 'x': extRatio = atof(optarg); break;
            case 'v': gVar = atof(optarg); break;
            case 't': pdfType = atoi(optarg); break;
            case 'c': datasetInference = true;
            case '?': break;                             /* getopt_long already printed an error message. */
            case 'h':
            default: help(); r = -1;
         }
    }
    //check the validity of parameters
    if(pdfType==FG::Gaussian)
    {
        if(gVar>=0.000001 && extRatio>=0.000001) r = 0; else r = -2; //since gVar and extRatio are double, than we don't check for equivalence with zero
    }
    if(nStates==0 || vRange==0 || pdfType==-1) r = -2;
    if(datasetInference)
    {
        if(datasetName.isEmpty() || datasetName.isNull())
            r = -2;
    }
    return r;
}

void help(void)
{
    printf("The following arguments must be supplied:\n");
    printf("--dataset=<Dataset_File_Name> or -d <Dataset_File_Name>\n");
    printf("--pdftype=<PDF> or -t <PDF>\n");
    printf("--nstates=<NS> or -n <NS>\n");
    printf("--vrange=<VR> or -r <VR>\n");
    printf("And for Gaussian pdf, the following arguments must also be supplied:\n");
    printf("--extratio=<XR> or -x <XR>\n");
    printf("--gvariance=<GV> or -v <GV>\n\n");
    printf("The following pdfType is valid:\n");
    printf("\t0 - Single\n");
    printf("\t1 - Gaussian\n");
    printf("\t2 - Binary\n");
    printf("\t3 - Beta\n\n");
    printf("For inference mode, use -c to use dataset for inference\n\n");
}
