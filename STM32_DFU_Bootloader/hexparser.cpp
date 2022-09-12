#include "hexparser.h"
#include <QDebug>

hexParser::hexParser(QString filePath, bool isWrite)
{
    file = new QFile (filePath); // generate error
    if (!file->exists())
    {
        isError = true;
#ifdef DEBUG
        qDebug()<<"File does not exist!";
#endif
    }

    if(!isWrite)
    {
        if (!file->open(QIODevice::ReadOnly))
        {
            isError = true;
#ifdef DEBUG
            qDebug()<<"Something wrong with open file";
#endif
        }

        this->init();
    }
}

hexParser::~hexParser()
{
    delete (file);
}

/**
 * @brief hexParser::init Читает из файла, проверяет файл на intel hex сигнатуру, читает старшее слово смещения адресса HBYTEADDRESS смещает указатель на начало следующей строчки
 * @details Вызвать перед использованием
 */
void hexParser::init()
{
    bArray = file->readAll();
    if (bArray.at(0)!=':')
    {
        qDebug() <<"IncorrectFile";
        isError = true;
        return;
    }
    this->pointer = 1;
    uint8_t b[32];
    getByteSlice(this->bArray, 2, this->pointer, b);
    int num =this->numFromAsciiByte(b,2);
    if (num!=2)
    {
        qDebug()<<"WrongFileFormat";isError = true;
        return;
    };
    this->pointer+=2;
    this->pointer+=6;
    getByteSlice(this->bArray, 4, this->pointer, b);
    num =this->numFromAsciiByte(b,4);
    this->HBYTEADDRESS = num<<16;
#ifdef DEBUG
    qDebug()<< this->HBYTEADDRESS;
#endif
    this->pointer+=6;
    this->pointer+=2;

}
/*| start record byte|byte num- byte|address LW - 2bytes |record type | data  |
 *| ":" 0x3A         |usually 0x10  |start with 5000     |usually 00  |  32c  |
 *|         1c       |       2c     |          4c        |     2c     | 32c(4)|
 */


/**
 * @brief hexParser::numFromAsciiByte число из асsii байтов
 * @param b массив байт
 * @param его длина
 * @return число полученное из массива байт
 */
int hexParser::numFromAsciiByte(uint8_t *b, int len)
{
    std::string ACSII ((const char*)b,len);
    return  std::stoi (ACSII,0,16);
}
/*
 *
 * Входные параметры:
 * array - экземпляр QByteArray, массив в котором нужно сделать срез
 * len - длина среза
 * pointer - смещение среза от начала
 * Выходные параметры:
 * массив байт, которые являются срезом
 */
/**
 * @brief hexParser::getByteSlice производит срез
 * @param array эземпляр QByteArray, массив в котором нужно сделать срез
 * @param len длина среза
 * @param pointer смещение среза от начала
 * @param b  возвращает срез массива байт QByteArray в виде Byte*
 * @return True в случае успеха
 */
bool hexParser::getByteSlice(QByteArray array, int  len, int pointer, uint8_t *b)
{
    for (int i = 0; i < len ; i++ )
    {
        b[i] = array.at(pointer+i);
    }
    return true;
}

/**
 * @brief hexParser::getByteSliceSTACK производит срез
 * @param data - массив байт в котором необходимо сделать срез
 * @param len - длина среза
 * @param pointer - смещения среза от начала массива
 * @param out - полученный срез
 * @return True в случае успеха
 */
bool hexParser::getByteSliceSTACK(uint8_t *data, int  len, int pointer, uint8_t *out)
{
    for (int i = 0; i < len ; i++ ){
        out[i] = data[pointer+i];
    }
    return true;
}
/*
 * возвращает строку хекс файла
 * Выходные параметры(аргументы функции):
 * address - указатель на адресс, туда кладется аддресс записи
 * data - указатель на массив полубайт данной записи
 * len - указатель на длинну массива полубайт
 * Выходные параметры:
 * bool = true если читается запись, false если было прочино что-то другое
 */
/**
 * @brief hexParser::getNextLine парсит строчку hex файла
 * @param address - указатель на переменную, туда запишется адресс строки
 * @param data - сырые данные полученые при парсинге
 * @param len - их длина с учетом кодировки ascii
 * @return true в случае успеха, false если строчки закончились или прочитано что-то другое, например extended address
 */
bool hexParser::getNextLine(int *address, uint8_t *data, int* len)
{
    this->pointer+=1;
    uint8_t b [32];
    getByteSlice(this->bArray, 2,this->pointer,b);
    int num =this->numFromAsciiByte(b,2);
    // delete [] b;
    *(len) = num;
    if (num==0)
        return false; // eof may be tehre i need to set flag

    this->pointer+=2;
    uint8_t b2 [32];
    getByteSlice(this->bArray, 4,this->pointer,b2);
    *address = this->numFromAsciiByte(b2,4);

    // delete[] b2;
    this->pointer+=4;
    uint8_t b3[32];
    getByteSlice(this->bArray, 2,this->pointer,b3);
    int type = this->numFromAsciiByte(b3,2);

    this->pointer+=2;
    qDebug()<<"type ="
           << type;
    if ((type != 00) && (type != 0xF) && (type != 0x04))
    {
        this->pointer += 2*num+4;
        return false;
    } // it cause because type of data no writing (it may me eof or addition memory or extended segment of address or start application address)
    if (type == 0x0F)
    {
        uint8_t dataASCII[8] = {0};
        uint8_t dataCS[32] = {0};
        getByteSlice(this->bArray, 2*num, this->pointer, dataASCII); // remember delete then
        bytesFromAsciiByte(dataASCII, 2*num, dataCS); // segment fault
        uint32_t checksumP = (uint32_t)(dataCS[0]+(dataCS[1]<<4)+(dataCS[2]<<8)+(dataCS[3]<<12)+(dataCS[4]<<16)+(dataCS[5]<<20)+(dataCS[6]<<24)+(dataCS[7]<<28));
        qDebug()<<"Checksum = " << checksumP;
        this->checkSum = checksumP;
        this->pointer += 12;
        return false;
    }
    if(type == 0x04) // extended linear address record
    {
        int address_major_part = 0;
        uint8_t b4[4];
        getByteSlice(this->bArray, 4, this->pointer, b4);
        address_major_part = this->numFromAsciiByte(b4, 4)<<16;
        this->pointer+=2*num+4;
        (*len) = 0;
        *address = address_major_part;
        return true;
    }

    uint8_t  dataASCII[32];
    getByteSlice(this->bArray, 2*num, this->pointer, dataASCII); // remember delete then
    bytesFromAsciiByte(dataASCII, 2*num,data); // segment fault

    this->pointer += 2*num+4;
    return true;
}
/*
 *Переводит закодированые в ASCI полубайты в массив байт
 *входные параметры:
 *b - массив закодированых полубайт
 *len - его длина
 *выходные параметры:
 *decode - массив разкодированых полубайт
 */
/**
 * @brief hexParser::bytesFromAsciiByte Переводит закодированые в ASCI полубайты в массив байт
 * @param b - массив закодированных байт
 * @param len - его длина
 * @param decode массив декодированных полубайт
 * @return  всегда true
 */
bool hexParser::bytesFromAsciiByte (uint8_t *b, int len, uint8_t *decode)
{
    for (int i = 0 ; i < len ; i++)
    {
        uint8_t temp_b[32] ;
        getByteSliceSTACK(b,1,i,temp_b);
        uint8_t temp_ = this->numFromAsciiByte(temp_b,1);
        decode[i] = temp_;
    }
    return true;
}
/**
 * @brief hexParser::parseFile публичная функция для парсинга всего файла целиком
 * @param map - указатель на экземпляр QMap, там будет храниться структура <аддресс>:<массив байт QByteArray>
 * @return
 */
bool hexParser::parseFile(QMap <int,QByteArray*>* map)
{
    int address;
    int len;
    uint8_t data[32] ;


    int last_address = 0;
    int current_address = 0;
    while (getNextLine(&address,data, &len))
    {
        if ((last_address+len) != address)
        {
            uint8_t d[32];
            if(len > 0)
            {
                this->getFullByteArrayFromHalf((data), 2*len, d);
                QByteArray* part = new QByteArray((const char*)(d),len);
                current_address = address;
                last_address = address;
                map->insert(current_address + this->HBYTEADDRESS, part);
                qDebug () << "Найдена новая страница по адрессу";
                qDebug() << current_address + this->HBYTEADDRESS ;
            }
            else
            {
                this->HBYTEADDRESS = address;
            }
        }
        else
        {
            //there add a data to old page
            last_address = address;
            uint8_t d[32];
            this->getFullByteArrayFromHalf(data,2*len,d);
            QByteArray* array=map->value(current_address+this->HBYTEADDRESS);
            array->append((const char*)d,len);
        }
    }
    return true;
}

/**
 * @brief hexParser::getFullByteArrayFromHalf Переводит массив байт в ASCII коде в массив байт в декодированом виде
 * @param half массив полубайт в ASCII кодах
 * @param len его длина
 * @param out Массив декодированных байт, длиной len/2
 * @return всегда true
 */
bool hexParser::getFullByteArrayFromHalf(uint8_t* half, int len, uint8_t  out[])
{
    for (int i = 0 ; i < len/2; i++)
    {
        out[i] = (uint8_t)((half[2*i]<<4)|(half[2*i+1]));
    }
    return true;
}
/**
 * @brief hexParser::compare сравнивает два массива байт
 * @param data1 первый массив типа byte
 * @param data2 второй массив типа byte
 * @param len их длина
 * @return если обнаружено хотя бы одно не совпадение false, если все байты совпадают - true
 */
bool hexParser::compare(uint8_t *data1 , uint8_t *data2, int len)
{
    for (int i = 0; i < len; i ++)
    {
        if (data1[i] != data2[i])
        {
#ifdef DEBUG
            qDebug ()<< "do not match at ";
            qDebug()<<i;
#endif
            return false;
        }
    }
    return true;
}
/**
 * @brief hexParser::createHexFile создает новый hex файл на основе старого
 * @param map - Qmap указатель, в нем хранится адресс и набор байт для hexфайла
 * @param path - путь, по которому он должен быть сохранен
 * @return
 */
bool hexParser::createHexFile(QMap<int, QByteArray*> *map, QString path)
{
    if (map == NULL) return false;
    if (path == "") return false;
    // make copy
    this->encmap = *map;
    // create file
    QFile hex(path);
    if (!hex.open(QIODevice::ReadWrite))
    {
        return  false;
    }
    //write extended adress:
    QList<int> keys = encmap.keys();
    int address = keys.at(0);
    int startAddress = address;
    int extended = (address & 0xFFFF0000)>>16;
    uint8_t data[]  = {(uint8_t)((extended & 0xFF00)>>8),(uint8_t)(extended & 0xFF)};
    this->addLineToHexFile(&hex, 2, 0, 4, data);
    // only for one address hex
    for (int address : keys)
    {
       QByteArray* ba = encmap.value(address);
       uint8_t* data = (uint8_t*)ba->data();

       for (int i = 0 ; i < ba->size(); i += 16)
       {
           if((address+i) > startAddress && ((address+i) % 0x10000) == 0) // add Extended Linear Address Record
           {
               extended = ((address+i) & 0xFFFF0000)>>16;
               uint8_t ext_data[]  = {(uint8_t)((extended & 0xFF00)>>8),(uint8_t)(extended & 0xFF)};
               addLineToHexFile(&hex, 2, 0, 4, ext_data);
           }
           addLineToHexFile(&hex, 16, (address+i) & 0xFFFF, 0, &(data[i]));
       }

       // write balance
       int balance_len = ba->size()%16;
       if (balance_len!=0)
       addLineToHexFile(&hex, balance_len, address +(int)(ba->size()/16), 0, &(data[0+(int)(ba->size()/16)]));
    }
    //write EOF
    this->addLineToHexFile(&hex, 0, 0, 1, NULL);

    hex.flush();
    hex.close();
    return true;
}

bool hexParser::addLineToHexFile (QFile *file, int byte_num, int address, int type, uint8_t *data)
{
 // generate check sum
    int temp_sum=0;
    temp_sum += byte_num & 0xFF;
    // 8 - 15 bit address for sum
    temp_sum += (address & 0xFFFF)>>8;
    // 0 - 7 bit adrees for sum
    temp_sum += address & 0xFF;
    temp_sum+= type & 0xFF;
    for (int i  = 0; i < byte_num; i++)
    {
        temp_sum += (data[i]) & 0xFF;
    }
    uint8_t checksum = (uint8_t)(0-temp_sum);
    // start writing line
    //first is ":"
    QString temp_s = ":";
    file->write(temp_s.toLatin1());
    //next writing byte num
    temp_s = (QString::number(byte_num, 16)).toUpper();
    if (byte_num < 16) temp_s = "0"+temp_s;
    file->write(temp_s.toLatin1());
    // writin address low byte
    int temp_address = (address & 0xFFFF);

    temp_s = QString::number(temp_address,16).toUpper();
    if (temp_address==0) temp_s = "0000";
    else
    {
        if (temp_address<0x1000) temp_s = "0"+temp_s;
        if (temp_address<0x0100) temp_s = "0"+temp_s;
    }
    file->write(temp_s.toLatin1());
    //writin type
    temp_s = QString::number(type, 16).toUpper();
    temp_s = "0"+temp_s;//there is bug if using type more than 0xF
    file->write(temp_s.toLatin1());
    // writing data
    for (int i = 0 ; i < byte_num; i++)
    {
        temp_s = QString::number(data[i], 16).toUpper();
        if (data[i]==0)temp_s= "00";
        else
        if (data[i]<0x10)temp_s = "0"+temp_s;
        file->write(temp_s.toLatin1());
    }
    //writing check sum
    temp_s  = QString::number(checksum, 16).toUpper();
    if (checksum==0)temp_s = "00";
    else
        if (checksum<0x10) temp_s = "0"+temp_s;
    file->write(temp_s.toLatin1());
    //writing end byte

    QByteArray ba;
    ba.append(0x0D);
    ba.append(0x0A);
    file->write(ba);

    return true;
}
/**
 * @brief hexParser::getCheckSum
 * @return возращает 32битное число - чексумма, полученная в ходе парсинга файла
 */
uint32_t hexParser::getCheckSum()
{
    return this->checkSum;
}
