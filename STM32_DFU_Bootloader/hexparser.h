#ifndef HEXPARSER_H
#define HEXPARSER_H
#include <QFile>
#include <QMap>
#define DEBUG
/** @file
 *  @brief Содержит публичные методы для парсинга hex-файла
 *
 *
 *
 */
/**
 * @brief The hexParser class класс для парсинга .hex файлов
 */
class hexParser
{
public:

    hexParser(QString filePath, bool isWrite);
    ~hexParser();
    bool parseFile(QMap<int,QByteArray*>*);
    bool isError = false;
    bool createHexFile(QMap<int,QByteArray*>* map, QString path);
    uint32_t getCheckSum();


private:
    uint32_t checkSum=0;
    QFile* file;
    QMap<int, QByteArray*> encmap;
    void init ();
    QByteArray bArray;
    int HBYTEADDRESS;
    int pointer=0;
    int parsedCheckSum = 0;
    bool isBin = false;

    int numFromAsciiByte (uint8_t* b, int len);
    bool getByteSlice(QByteArray array, int  len, int pointer, uint8_t* out);
    bool getByteSliceSTACK (uint8_t* data, int len , int pointer, uint8_t* out);
    bool getNextLine(int* address, uint8_t* data, int* len);
    bool bytesFromAsciiByte (uint8_t* b, int len, uint8_t*);
    bool getFullByteArrayFromHalf(uint8_t* half, int len, uint8_t* out);
    bool compare (uint8_t* data1 , uint8_t* data2, int len) ;
    bool addLineToHexFile (QFile *file, int byte_num, int address, int type, uint8_t* data);
};

#endif // HEXPARSER_H
