#include "MRCParser.h"

/** https://www.ccpem.ac.uk/mrc_format/mrc2014.php
Word	Bytes	Variable name	Description	Note
1		1-4		NX				number of columns in 3D data array (fast axis)	1
2		5-8		NY				number of rows in 3D data array (medium axis)
3		9-12	NZ				number of sections in 3D data array (slow axis)
4		13-16	MODE	0 8-bit signed integer (range -128 to 127)
						1 16-bit signed integer
						2 32-bit signed real
						3 transform : complex 16-bit integers
						4 transform : complex 32-bit reals
						6 16-bit unsigned integer	2
5		17-20	NXSTART			location of first column in unit cell
6		21-24	NYSTART			location of first row in unit cell
7		25-28	NZSTART			location of first section in unit cell
8		29-32	MX				sampling along X axis of unit cell
9		33-36	MY				sampling along Y axis of unit cell
10		37-40	MZ				sampling along Z axis of unit cell	3
11-13	41-52	CELLA			cell dimensions in angstroms
14-16	53-64	CELLB			cell angles in degrees
17		65-68	MAPC			axis corresp to cols (1,2,3 for X,Y,Z)	4
18		69-72	MAPR			axis corresp to rows (1,2,3 for X,Y,Z)
19		73-76	MAPS			axis corresp to sections (1,2,3 for X,Y,Z)
20		77-80	DMIN			minimum density value	5
21		81-84	DMAX			maximum density value
22		85-88	DMEAN			mean density value
23		89-92	ISPG			space group number	6
24		93-96	NSYMBT			size of extended header (which follows main header) in bytes	7
25-49	97-196	EXTRA			extra space used for anything - 0 by default
27		105		EXTTYP			code for the type of extended header	8
28		109		NVERSION		version of the MRC format	9
50-52	197-208	ORIGIN			phase origin (pixels) or origin of subvolume (A)	10
53		209-212	MAP				character string 'MAP ' to identify file type
54		213-216	MACHST			machine stamp encoding byte ordering of data	11
55		217-220	RMS	rms			deviation of map from mean density
56		221-224	NLABL			number of labels being used
57-256	225-1024	LABEL(20,10)	10 80-character text labels
*/

//Need to remove Qt5 dependency.
//void MRCParser::exportHeader(QString filename)
//{
//    QString header;
//
//    QFile file(filename);
//
//    if (!file.open(QFile::ReadOnly)) {
//		printf("Could not open file");
//    }
//
//    QDataStream in(&file);
//    in.setByteOrder(QDataStream::LittleEndian);
//    in.setFloatingPointPrecision(QDataStream::SinglePrecision); // 4 bytes per float!!!
//
//    header = "Header format description\n"
//		"======================================================================\n"
//		"Word	Bytes	Variable name	Description	Note\n"
//    "1		1-4		NX				number of columns in 3D data array (fast axis)	\n"
//    "2		5-8		NY				number of rows in 3D data array (medium axis)	\n"
//    "3		9-12	NZ				number of sections in 3D data array (slow axis)	\n"
//    "4		13-16	MODE	0 8-bit signed integer (range -128 to 127)\n"
//    "                        1 16-bit signed integer\n"
//    "                        2 32-bit signed real\n"
//    "                        3 transform : complex 16-bit integers\n"
//    "                        4 transform : complex 32-bit reals\n"
//    "                        6 16-bit unsigned integer	\n"
//    "5		17-20	NXSTART			location of first column in unit cell	\n"
//    "6		21-24	NYSTART			location of first row in unit cell	\n"
//    "7		25-28	NZSTART			location of first section in unit cell	\n"
//    "8		29-32	MX				sampling along X axis of unit cell	\n"
//    "9		33-36	MY				sampling along Y axis of unit cell	\n"
//    "10		37-40	MZ				sampling along Z axis of unit cell	\n"
//    "11-13	41-52	CELLA			cell dimensions in angstroms	\n"
//    "14-16	53-64	CELLB			cell angles in degrees	\n"
//    "17		65-68	MAPC			axis corresp to cols (1,2,3 for X,Y,Z)	\n"
//    "18		69-72	MAPR			axis corresp to rows (1,2,3 for X,Y,Z)	\n"
//    "19		73-76	MAPS			axis corresp to sections (1,2,3 for X,Y,Z)	\n"
//    "20		77-80	DMIN			minimum density value	\n"
//    "21		81-84	DMAX			maximum density value	\n"
//    "22		85-88	DMEAN			mean density value	\n"
//    "23		89-92	ISPG			space group number	\n"
//    "24		93-96	NSYMBT			size of extended header (which follows main header) in bytes	\n"
//    "25-49	97-196	EXTRA			extra space used for anything - 0 by default	\n"
//    "27		105		EXTTYP			code for the type of extended header	\n"
//    "28		109		NVERSION		version of the MRC format	\n"
//    "50-52	197-208	ORIGIN			phase origin (pixels) or origin of subvolume (A)	\n"
//    "53		209-212	MAP				character string 'MAP ' to identify file type	\n"
//    "54		213-216	MACHST			machine stamp encoding byte ordering of data	\n"
//    "55		217-220	RMS	rms			deviation of map from mean density	\n"
//    "56		221-224	NLABL			number of labels being used	\n"
//    "57-256	225-1024	LABEL(20,10)	10 80-character text labels\n\n";
//
//	QFileInfo fileInfo(file.fileName());
//	header += fileInfo.fileName() + " HEADER\n";
//	header += "======================================================================\n";
//
//	nx = ParsingHelper::readInt(in);
//	ny = ParsingHelper::readInt(in);
//	nz = ParsingHelper::readInt(in);
//    header += QString("NX:%1\n").arg(nx);
//    header += QString("NY:%1\n").arg(ny);
//    header += QString("NZ:%1\n").arg(nz);
//
//	mode = ParsingHelper::readInt(in);
//	header += QString("MODE:%1\n").arg(mode);
//
//	nxstart = ParsingHelper::readInt(in);
//	nystart = ParsingHelper::readInt(in);
//	nzstart = ParsingHelper::readInt(in);
//	header += QString("NXSTART:%1\n").arg(nxstart);
//	header += QString("NYSTART:%1\n").arg(nystart);
//	header += QString("NZSTART:%1\n").arg(nzstart);
//
//	mx = ParsingHelper::readInt(in);
//	my = ParsingHelper::readInt(in);
//	mz = ParsingHelper::readInt(in);
//    header += QString("MX:%1\n").arg(mx);
//    header += QString("MY:%1\n").arg(my);
//    header += QString("MZ:%1\n").arg(mz);
//
//    int dumpI;
//    float dumpF;
//
//    // CELLA - size in angstrom
//    in >> cellsx;
//    in >> cellsy;
//    in >> cellsz;
//    header += QString("CELLX:%1\n").arg(cellsx);
//    header += QString("CELLY:%1\n").arg(cellsy);
//    header += QString("CELLZ:%1\n").arg(cellsz);
//
//    // CELLB - angles in angstrom
//    in >> alpha;
//    in >> beta;
//    in >> gamma;
//    header += QString("ALPHA:%1\n").arg(alpha);
//    header += QString("BETA:%1\n").arg(beta);
//    header += QString("GAMMA:%1\n").arg(gamma);
//
//    // MAPC, MAPR, MAPS
//    in >> mapc;
//    in >> mapr;
//    in >> maps;
//    header += QString("MAPC:%1\n").arg(mapc);
//    header += QString("MAPR:%1\n").arg(mapr);
//    header += QString("MAPS:%1\n").arg(maps);
//
//    in >> dmin;
//    in >> dmax;
//    in >> dmean;
//    header += QString("DMIN:%1\n").arg(dmin);
//    header += QString("DMAX:%1\n").arg(dmax);
//    header += QString("DMEAN:%1\n").arg(dmean);
//
//    // ISPG
//    //Spacegroup 0 implies a 2D image or image stack.For crystallography, ISPG represents the actual spacegroup.For single volumes from EM / ET, the spacegroup should be 1. For volume stacks, we adopt the convention that ISPG is the spacegroup number + 400, which in EM / ET will typically be 401.
//    in >> spacegroup;
//    header += QString("SPACEGROUP:%1\n").arg(spacegroup);
//
//    in >> extheader;
//    header += QString("EXTHEADER:%1\n").arg(extheader);
//
//    //auto buffer = readBytes(in, 100);
//
//    // EXTRA
//    for (int i = 0; i < 23; i++) {
//        in >> dumpI;
//    }
//
//    // EXTTYP
//    exttyp = ParsingHelper::readString(in, 4).toStdString();
//	QString exttypqs = exttyp.c_str();
//    header += QString("EXTTYP:%1\n").arg(exttypqs.trimmed());
//
//    // NVERSION
//    in >> version;
//    header += QString("VERSION:%1\n").arg(version);
//
//    // ORIGIN
//    in >> x;
//    in >> y;
//    in >> z;
//    header += QString("X:%1\n").arg(x);
//    header += QString("Y:%1\n").arg(y);
//    header += QString("Z:%1\n").arg(z);
//
//    // MAP
//    auto map = ParsingHelper::readString(in, 4);
//    header += QString("MAP:%1\n").arg(map/*.trimmed()*/);
//
//    //MACHST
//	machst = ParsingHelper::readInt(in);
//	header += QString("MACHST:%1\n").arg(machst);
//    //in >> dumpI;
//
//    //RMS
//	in >> rms;
//	header += QString("RMS:%1\n").arg(rms);
//    //in >> dumpF;
//
//    // NLABL
//    auto labelsCount = ParsingHelper::readInt(in);
//    header += QString("LABELSCOUNT:%1\n").arg(labelsCount);
//
//    for (int i = 0; i < 10; i++)
//    {
//		QString label = ParsingHelper::readString(in, 80).trimmed().remove(QChar::Null);
//        header += QString("LABEL:%1\n").arg(label);
//        labels.push_back(label.toStdString());
//    }
//
//	const QString qPath("header.txt");
//	QFile qFile(qPath);
//	if (qFile.open(QIODevice::WriteOnly)) {
//		QTextStream out(&qFile); out << header;
//		qFile.close();
//	}
//}

int MRCParser::load_original(string fileName)
{
	printf("loading file\n");
	FILE* file;
	fopen_s(&file,fileName.c_str(), "r");

	if (file == NULL)
	{
		printf("Error reading MRC file.\n");
		exit(0);
	}

	printf("loaded file\n");

	fread(&nx, 4, 1, file);
	fread(&ny, 4, 1, file);
	fread(&nz, 4, 1, file);
	fread(&mode, 4, 1, file);
	printf("odim: %d, %d, %d\n", nx, ny, nz);
	printf("mode: %d\n", mode);

	if (!((mode == 0) || (mode == 1) || (mode == 2) || (mode == 6)))
	{
		printf("MRC mode %d not supported yet!\n", mode);
		return -1;
	}

	fread(&nxstart, 4, 1, file);
	fread(&nystart, 4, 1, file);
	fread(&nzstart, 4, 1, file);
	printf("start: %d, %d, %d\n", nxstart, nystart, nzstart);

	fread(&mx, 4, 1, file);
	fread(&my, 4, 1, file);
	fread(&mz, 4, 1, file);
	printf("grid dim: %d, %d, %d\n", mx, my, mz);

   //To dump unwanted data from header
	int dump = 0;

	// CELLA - size in angstrom
	fread(&cellsx, 4, 1, file);
	fread(&cellsy, 4, 1, file);
	fread(&cellsz, 4, 1, file);
	//printf("cells dim: %f, %f, %f\n", cellsx, cellsy, cellsz);

   // CELLB - angles in angstrom
	fread(&alpha, 4, 1, file);
	fread(&beta, 4, 1, file);
	fread(&gamma, 4, 1, file);
	//printf("cell angles: %f, %f, %f\n", alpha, beta, gamma);

   // MAPC, MAPR, MAPS
	fread(&mapc, 4, 1, file);
	fread(&mapr, 4, 1, file);
	fread(&maps, 4, 1, file);
	//printf("map: %d, %d, %d\n", mapc, mapr, maps);

   //dmin, dmax, dmean
	fread(&dmin, 4, 1, file);
	fread(&dmax, 4, 1, file);
	fread(&dmean, 4, 1, file);
	//printf("mmm: %f, %f, %f\n", dmin, dmax, dmean);

   // ISPG
   //Spacegroup 0 implies a 2D image or image stack.For crystallography, ISPG represents the actual spacegroup.For single volumes from EM / ET, the spacegroup should be 1. For volume stacks, we adopt the convention that ISPG is the spacegroup number + 400, which in EM / ET will typically be 401.
	fread(&spacegroup, 4, 1, file);
	//printf("ISPG: %d\n", spacegroup);

   //Extended header size in bytes
	fread(&extheader, 4, 1, file);
	//printf("exthdr: %d\n", extheader);

   // EXTRA SPACE
	for (int i = 0; i < 2; i++)
	{
		fread(&dump, 4, 1, file);
	}

	// EXTTYP
	char c4[4];
	fread(&c4[0], 1, 4, file);
	exttyp = string(&c4[0], 4);
	//printf("exttyp: %s\n", exttyp.c_str());

   // NVERSION
	fread(&version, 4, 1, file);
	//printf("version: %d\n", version);

   // EXTRA SPACE, why 23?
	for (int i = 0; i < 21; i++)
	{
		fread(&dump, 4, 1, file);
	}

	// ORIGIN
	fread(&x, 4, 1, file);
	fread(&y, 4, 1, file);
	fread(&z, 4, 1, file);
	//printf("origin: %f, %f, %f\n", x, y, z);

   // MAP
	fread(&c4[0], 1, 4, file);
	string map = string(&c4[0], 4);
	//printf("MAP: %s\n", map.c_str());

   // MACHST
	fread(&machst, 4, 1, file);

	// RMS
	fread(&rms, 4, 1, file);

	// NLABL
	int labelsCount;
	fread(&labelsCount, 4, 1, file);
	//printf("labelscount: %d\n", labelsCount);

	char label[80];
	for (int i = 0; i < 10; i++)
	{
		fread(&label, 80, 1, file);
		//printf("label: %s\n", label);
		labels.push_back(string(label));
	}

	fclose(file);

	fdataLength = nx * ny * nz;
	fdata = new float[fdataLength];

	if (mode == 0)
	{
		vector<int8_t> myVec(fdataLength);
		std::ifstream f(fileName.c_str(), std::ios::binary);
		f.seekg(1024 + extheader);
		f.read(reinterpret_cast<char*>(&myVec.front()), fdataLength * sizeof(int8_t));
		f.close();

		for (int i = 0; i < myVec.size(); i++)
		{
			fdata[i] = (float)myVec[i];
		}
		myVec.clear();
	}
	else if (mode == 1)
	{
		vector<int16_t> myVec(fdataLength);
		std::ifstream f(fileName.c_str(), std::ios::binary);
		f.seekg(1024 + extheader);
		f.read(reinterpret_cast<char*>(&myVec.front()), fdataLength * sizeof(int16_t));
		f.close();

		for (int i = 0; i < myVec.size(); i++)
		{
			fdata[i] = (float)myVec[i];
		}
		myVec.clear();
	}
	else if (mode == 2)
	{
		vector<float> myVec(fdataLength);
		std::ifstream f(fileName.c_str(), std::ios::binary);
		f.seekg(1024 + extheader);
		f.read(reinterpret_cast<char*>(&myVec.front()), fdataLength * sizeof(float));
		f.close();

		for (int i = 0; i < myVec.size(); i++)
		{
			fdata[i] = myVec[i];
		}
		myVec.clear();
	}
	else if (mode == 6)
	{
		vector<uint16_t> myVec(fdataLength);
		std::ifstream f(fileName.c_str(), std::ios::binary);
		f.seekg(1024 + extheader);
		f.read(reinterpret_cast<char*>(&myVec.front()), fdataLength * sizeof(uint16_t));
		f.close();

		for (int i = 0; i < myVec.size(); i++)
		{
			fdata[i] = (float)myVec[i];
		}
		myVec.clear();
	}
	printf("** MRCParser: MRC file process sucessfully!\n");
	return 0;
}


//Pause regardless of OS
void _pause()
{
#ifdef _WIN32
	system("pause");
#else
	printf("Press enter to continue..."); getchar(); printf("\n");
#endif
}

//Saves host data object with dimension dim
//name requries extension .mrc
//ISPG = 0 for 2D stacks, 1 for volumes
void smrc(std::string filepath, float* data, int dim[3], int ISPG , float min , float max , float mean )
{
	//Open binary(!!) file
	FILE* file;
	fopen_s(&file,filepath.c_str(), "wb");

	if (file == NULL)
	{
		printf("Error creating MRC file.\n");
		_pause();
		fclose(file);
	}

	int data_size = dim[0] * dim[1] * dim[2];

	//nx, ny, nz
	int d[3] = { dim[0], dim[1], dim[2] };
	fwrite(d, 4, 3, file);
	//fwrite((const void*)dim.x, 4, 1, file);
	//fwrite((const void*)dim.y, 4, 1, file);
	//fwrite((const void*)dim.z, 4, 1, file);

	//MODE 2 for float data
	int dos[1] = { 2 };
	fwrite(dos, 4, 1, file);

	//NX, NY, NZ start
	int zero[1] = { 0 };
	for (int i = 0; i < 3; i++) {
		fwrite(zero, 4, 1, file);
	}

	//MX = ?
	//fwrite(&dim.x, 4, 1, file);
	//MY = ?
	//fwrite(&dim.y, 4, 1, file);
	//MZ = 1
	//fwrite(&dim.z, 4, 1, file);
	fwrite(d, 4, 3, file);

	//CELLX	
	fwrite(zero, 4, 1, file);
	//CELLY
	fwrite(zero, 4, 1, file);
	//CELLZ
	fwrite(zero, 4, 1, file);

	//alpha
	float f[1] = { 90.0f };
	fwrite(f, 4, 1, file);
	//beta
	fwrite(f, 4, 1, file);
	//gamma
	fwrite(f, 4, 1, file);

	//MAPC
	int udt[3] = { 1, 2, 3 };
	//fwrite((const void*)1, 4, 1, file);
	//MAPR
	//fwrite((const void*)2, 4, 1, file);
	//MAPS
	//fwrite((const void*)3, 4, 1, file);
	fwrite(udt, 4, 3, file);

	//DMIN
	f[0] = min;
	fwrite(f, 4, 1, file);

	//DMAX
	f[0] = max;
	fwrite(f, 4, 1, file);

	//DMEAN
	f[0] = mean;
	fwrite(f, 4, 1, file);

	//ISPG = 1 for Single EM/ET volumes
	int ispg[1] = { ISPG };
	fwrite(ispg, 4, 1, file);

	//NSYMBT size of extended header
	fwrite(zero, 4, 1, file);

	//EXTRA
	for (int i = 0; i < 25; i++)
	{
		if (i == 2)
		{
			//EXTTYP
			char exttyp[4] = { 'J', 'R', 'Z', 'L' };
			fwrite(exttyp, 1, 4, file);
		}
		else if (i == 3)
		{
			//NVERSION
			int nver[1] = { 0715 };
			fwrite(nver, 4, 1, file);
		}
		else
		{
			fwrite(zero, 4, 1, file);
		}
	}

	//ORIGIN
	for (int i = 0; i < 3; i++) {
		fwrite(zero, 4, 1, file);
	}

	//MAP
	char map[4] = { 'M', 'A', 'P', ' ' };
	fwrite(map, 1, 4, file);

	//MACHST
	//0x44 0x44 0x00 0x00 for little endian machines, and 0x11 0x11 0x00 0x00 for big endian machines
	char MACHST[4] = { (char)'DD', (char)'DD', '\0', '\0' }; //Little endian
	fwrite(MACHST, 1, 4, file);

	//RMS
	float rms[1] = { 9.98f };
	fwrite(rms, 4, 1, file);

	//NLABL = 0
	int diez[1] = { 0 };
	fwrite(diez, 4, 1, file);

	//Write 10 80-char labels.
	unsigned char c[1] = { ' ' };
	for (int i = 0; i < 800; i++)
	{
		fwrite(c, 1, 1, file);
	}

	float dd[1];
	for (int i = 0; i < data_size; i++)
	{
		dd[0] = data[i];
		fwrite(dd, 4, 1, file);
	}

	fclose(file);
}
