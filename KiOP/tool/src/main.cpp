
//==========================================================================//
//============================ FICHIERS INCLUS =============================//

#include "Parametres.h"
#include "main.h"


//==========================================================================//
//=========================== VARIABLES GLOBALES ===========================//

// 0: Inactive; 1: in menu; 2: tool chosen
int currentState = 0;
int lastState = 0;
int moveCounter = 0;

// 0: Zoom; 1: Contrast; 2: Move
int currentTool = 3; 
int lastTool = 3;
int totalTools = 6; // +1
int positionTool[7]; //position des outils dans le menu
int afficheTool[7]; //affichage des outils

//Layout
int totalLayoutTools = 5; //+1
int currentLayout = 0;
int currentLayoutTool = 0;
int lastLayoutTool = 0;

float iconIdlePt = 192.0;
float iconActivePt = 64.0;
xn::Context context;
xn::DepthGenerator dpGen;
xn::DepthMetaData dpMD;
xn::HandsGenerator myHandsGenerator;
XnStatus status;

int depthRefVal = 0;
bool realDepth = false;
float lastX = 0.0;
float lastY = 0.0;
float lastZ = 0.0;
//number of frames after which we collect point data
int nFrames = 2; 
//actual frame number
int actualFrame = 0; 
int moveThreshold = 4;
float handDepthLimit;
float handSurfaceLimit;
// Param�tre de profondeur de la main
float handDepthThreshold = 50.0;
float handSurfaceThreshold = 50.0;
bool handDown = false;
int cptHand = 0;
int handFrames = 5;

bool handClosed = false;
bool handStateChanged = false;
bool handFlancMont = false;
bool handFlancDesc = false;
bool handClic = false;

bool toolSelectable = false;
bool methodeMainFermeeSwitch = false;

// NITE
bool activeSession = false;
bool steadyState = false;
bool steady2 = false;
bool timerOut = false;
XnVSessionManager *sessionManager;
XnVPointControl *pointControl;
XnPoint3D handPt;
XnPoint3D lastPt;
XnVSteadyDetector sd;
XnVSteadyDetector sd2;
XnVSteadyDetector sd3;
XnVSteadyDetector sd02;
XnVFlowRouter* g_pFlowRouter;
XnFloat g_fSmoothing = 0.2f;

// Qt
#ifdef _OS_WIN_
	int qargc = 0;
	char **qargv = NULL;
	QApplication app(qargc,qargv);
#endif
GraphicsView *window;
GraphicsView *windowActiveTool;
QGraphicsScene *sceneActiveTool;
GraphicsView *viewLayouts;
vector<Pixmap*> pix; //for main tools
vector<Pixmap*> pixL; //for layouts
Pixmap* pixActive; //for activeTool
TelnetClient telnet;

// Cursor objects //
Cursor cursor(2);

// KiOP//
//CursorQt cursorQt(2);
HandClosedDetection hCD;


//==========================================================================//
//============================== FONCTIONS =================================//

// Fonction d'initialisation
void Initialisation(void)
{
	cout << "= = = = = = = = = = = = = = = =" << endl;
	cout << "\tINITIALISATION" << endl;
	cout << "= = = = = = = = = = = = = = = =" << endl << endl;
	cout << "Resolution d'ecran : " << SCRSZWidth() << "x" << SCRSZHeight() << endl << endl;

	#ifdef _OS_WIN_
		ChangeCursor(0);
		InitGestionCurseurs();
	#endif
}

// Fonction de fermeture du programme
void CleanupExit()
{
	context.Shutdown();
	exit(1);
}



inline bool isHandPointNull()
{
	return ((handPt.X == -1) ? true : false);
}
bool detectLeft()
{
	return (((lastX-handPt.X) > moveThreshold) ? true : false);
}
bool detectRight()
{
	return (((lastX-handPt.X) < (-moveThreshold)) ? true : false);
}
bool detectForward()
{
	return (((lastZ-handPt.Z) > (moveThreshold)) ? true : false);
}
bool detectBackward()
{
	return (((lastZ-handPt.Z) < (-moveThreshold)) ? true : false);
}
bool detectClick()
{
	return (((lastZ-handPt.Z) > (5*moveThreshold)) ? true : false);
}


void chooseTool(int &currentTool, int &lastTool, int &totalTools)
{
	if ((lastX-handPt.X)>0)
	{
		if(moveCounter>0) 
			moveCounter = 0;
		moveCounter=moveCounter-(lastX-handPt.X);//+(handPt.Z/1000);
	}
	else if((lastX-handPt.X)<0)
	{
		if(moveCounter<0) 
			moveCounter = 0;
		moveCounter=moveCounter-(lastX-handPt.X);//+(handPt.Z/1000);
	}

	//cout << handPt.Z << endl;
	if (moveCounter<=-10)
	{
		//go left in the menu
		lastTool=currentTool;
		if (currentTool-1<0)
			currentTool = 0;
		else
			currentTool--;
		moveCounter = 0;
	}
	else if(moveCounter>=10)
	{
		//go right in the menu
		lastTool=currentTool;
		if (currentTool+1>totalTools)
			currentTool = totalTools;
		else
			currentTool++;
		moveCounter = 0;
	}
	else
	{
		//do nothing
	}
}

void browse(int currentTool, int lastTool, vector<Pixmap*> pix)
{
	//only set the pixmap geometry when needed 
	if (lastTool != currentTool)
	{
		if (currentTool == 0)
			pix.operator[](currentTool)->setGeometry(QRectF( (currentTool*128.0), iconIdlePt-64, 128.0, 128.0));//for zooming on the tool
		else
			pix.operator[](currentTool)->setGeometry(QRectF( (currentTool*128.0)-32, iconIdlePt-64, 128.0, 128.0));//for zooming on the tool
		pix.operator[](lastTool)->setGeometry(QRectF( lastTool*128.0, iconIdlePt, 64.0, 64.0));
	}
}

void handleState()
{
	//cout << currentState << endl;

	bool inIntervalleX = (handPt.X < handSurfaceLimit+handSurfaceThreshold)&&(handPt.X > handSurfaceLimit-handSurfaceThreshold); // Bool�en pour indiquer si la main est dans le bon intervelle de distance en X
	bool inIntervalleZ = (handPt.Z < handDepthLimit+handDepthThreshold); // Bool�en pour indiquer si la main est dans le bon intervelle de distance en Z

	
	switch(currentState)
	{
		case -2 :
			if (!handClosed) // ?
			{
				currentState = 1;
			}

		// L'outil a �t� s�lectionn�
		case -1 :
			if (lastState == 1)
			{
				for (int i=0; i<=totalTools; i++)
				{
					pix.operator[](i)->hide();
				} 
				windowActiveTool->show();
				//pixActive->show();
				//pixActive->load(QPixmap(":/images/Resources/_"+pix.operator[](currentTool)->objectName()+".png").scaled(128,128));
				pixActive->load(QPixmap(":/images/Resources/"+pix.operator[](currentTool)->objectName()+".png").scaled(128,128));
				windowActiveTool->setBackgroundBrush(QBrush(Qt::gray, Qt::SolidPattern));
				if (currentTool==totalTools)
					currentState=2;
				lastTool = currentTool;
				lastState = currentState;

				if ((currentTool != 1) || (currentTool != 6))
				{
					glutTimerFunc(1500,onTimerOut,12345);
				}
			}
			if (timerOut)
			{
				windowActiveTool->hide();
				currentState = 2;
				windowActiveTool->show();
				//pixActive->load(QPixmap(":/images/Resources/activeTool/_"+pix.operator[](currentTool)->objectName()+".png").scaled(128,128));
				windowActiveTool->setBackgroundBrush(QBrush(Qt::green, Qt::SolidPattern));
				timerOut = false;
				handDepthLimit = handPt.Z;
				handSurfaceLimit = handPt.X;
			}
			break;

		// En attente d'un coucou
		case 0 :
			steady2 = false;
			steadyState = false;
			break;

		// Coucou effectu�, afficher le menu
		case 1 :
			//cout << "x= " << handPt.X << " ; y= " << handPt.Y << " ; z= " << handPt.Z << " ;   " << endl;
			if (lastState == 0)
			{
				viewLayouts->hide();
				windowActiveTool->hide();
				for (int i=0; i<=totalTools; i++)
				{
					pix.operator[](i)->setGeometry(QRectF( i*128.0, iconIdlePt, 64.0, 64.0));
					pix.operator[](i)->show();
				}
				lastState = currentState;
				telnet.connexion();
			}

			if (!handClosed)
			{
				chooseTool(currentTool, lastTool, totalTools);
				browse(currentTool,lastTool, pix);
			}

			// Si la main est ferm�e, on choisi un outil
			//if (0)
			if (handClosed)
			{				
				lastState = currentState;
				currentState = -1;
				nFrames = 2;
				moveThreshold = 4;
				cout << "Tool selected" << endl;
				telnet.connexion();
			}

			//code � ameliorer/////////////////////////////
			//quitter la session lorsqu'on baisse la main
			handDown = false;
			if (handPt.Z < 1500)
			{
				if (handPt.Y > lastPt.Y+150)
				{
					handDown = true;
				}
			}
			else if((handPt.Z < 2100)&&(handPt.Z>=1500))
			{
				if (handPt.Y > lastPt.Y+130)
				{
					handDown = true;
				}
			}
			else if(handPt.Z > 2100)
			{
				if (handPt.Y > lastPt.Y+100)
				{
					handDown = true;
				}
			}
			if (handDown)
			{
				telnet.deconnexion();
				sessionManager->EndSession();
				currentState=0;
			}
			break;

		// L'outil a �t� selectionn�
		case 2 :
			windowActiveTool->setBackgroundBrush(QBrush(Qt::green, Qt::SolidPattern));
			switch(currentTool)
			{
				// Zoom
				case 3 :
					//if ((handPt.X < handSurfaceLimit+handSurfaceThreshold)&&(handPt.X > handSurfaceLimit-handSurfaceThreshold)) { //&& (handDepthLimit-40 < handPt.Z)){
					//if (handPt.Z < handDepthLimit+handDepthThreshold) 
					if (handClosed)
					{
						if (detectForward())
						{
							for (int i=0; i<3; i++)
							{
								telnet.sendCommand(QString("\r\ndcmview2d:zoom -i 1\r\n"));
							}
						}
						else if(detectBackward())
						{
							for (int i=0; i<3; i++)
							{
								telnet.sendCommand(QString("\r\ndcmview2d:zoom -d 1\r\n"));
							}
						}
						else { }
					}
					else
					{
						windowActiveTool->setBackgroundBrush(QBrush(Qt::red, Qt::SolidPattern));
					}
					break;

				// Contraste
				case 5 :
					if (handClosed)
					{
						telnet.sendCommand(QString("\r\ndcmview2d:wl -- %1 %2\r\n").arg((int)(lastX-handPt.X)*6).arg((int)(lastY-handPt.Y)*6));
					} 
					else
					{
						windowActiveTool->setBackgroundBrush(QBrush(Qt::red, Qt::SolidPattern));
					}
					break;

				// Translation
				case 2 :
					if (handClosed)
					{
						telnet.sendCommand(QString("\r\ndcmview2d:move -- %1 %2\r\n").arg((int)(lastX-handPt.X)*6).arg((int)(lastY-handPt.Y)*6));
					} 
					else
					{
						windowActiveTool->setBackgroundBrush(QBrush(Qt::red, Qt::SolidPattern));
					}
					break;

				// Scroll
				case 4 :
					//if (handPt.Z < handSurfaceLimit+handSurfaceThreshold) //&& (handDepthLimit-40 < handPt.Z)){
					//if (handPt.Z < handDepthLimit+handDepthThreshold) {
					if (handClosed)
					{
						if ((lastZ-handPt.Z)<0)
						{
							for (int i=0; i<-(lastZ-handPt.Z)/6; i++)
							{
								telnet.sendCommand(QString("\r\ndcmview2d:scroll -i 1\r\n"));
							}
						}
						else if((lastZ-handPt.Z)>0)
						{
							for (int i=0; i<(lastZ-handPt.Z)/6; i++)
							{
								telnet.sendCommand(QString("\r\ndcmview2d:scroll -d 1\r\n"));
							}
						}
						else { }
					} 
					else
					{
						windowActiveTool->setBackgroundBrush(QBrush(Qt::red, Qt::SolidPattern));
					}
					break;

				// Souris
				case 0 :
					if (!handClosed)
					{
						lastState = currentState;
						currentState = 3;						// MODE SOURIS
						cursor.NewCursorSession();
					}
					break;

				// Layouts (currentTool = 5 , -5, -55)
				case 1 :
					//window->hide();
					pix.operator[](currentTool)->hide();
					windowActiveTool->hide();
					viewLayouts->show();
					if (!handClosed)
					{
						currentTool = -5;
					}
					break;

				case -5:
					chooseTool(currentLayoutTool, lastLayoutTool, totalLayoutTools);
					browse(currentLayoutTool,lastLayoutTool, pixL);
					if (handClosed)
					{
						viewLayouts->hide();
						switch(currentLayoutTool)
						{
							case 0 :
								telnet.sendCommand(QString("\r\ndcmview2d:layout -i 1x1\r\n"));
								break;
							case 1 :
								telnet.sendCommand(QString("\r\ndcmview2d:layout -i 1x2\r\n"));
								break;
							case 2 :
								telnet.sendCommand(QString("\r\ndcmview2d:layout -i 2x1\r\n"));
								break;
							case 3 :
								telnet.sendCommand(QString("\r\ndcmview2d:layout -i layout_c1x2\r\n"));
								break;
							case 4 :
								telnet.sendCommand(QString("\r\ndcmview2d:layout -i layout_c2x1\r\n"));
								break;
							case 5 :
								telnet.sendCommand(QString("\r\ndcmview2d:layout -i 2x2\r\n"));
								break;
						}
					}
					break;

				// Croix (exit)
				case 6 :
					cout << lastTool << endl;
					sessionManager->EndSession();
					telnet.deconnexion();
					windowActiveTool->hide();
					viewLayouts->hide();
					//currentTool = 0;
					//currentState = 0;
					//switch(currentLayoutTool)
					break;
			}

			// Pour quitter l'outil et revenir dans le menu
			if ( ( steadyState && (!handClosed) && (currentState != 3) && (currentTool != -5) ) 
					|| ((currentTool == -5) && (handClosed)) )
			{
				lastState = currentState;
				currentState = 1;
				nFrames = 2;
				moveThreshold = 4;
				windowActiveTool->hide();
				viewLayouts->hide();
				for (int i=0; i<=totalTools; i++)
				{
					pix.operator[](i)->setGeometry(QRectF( i*128.0, iconIdlePt, 64.0, 64.0));
					pix.operator[](i)->show();
				}
				currentTool=totalTools;
				lastTool=0;
				lastPt = handPt;
				//steadyState = false;
			}
			break;

		// Mouse control
		case 3 :

			// Sortie du mode souris
			if (cursor.GetState() == 0)
			{
				lastState = currentState;
				currentState = 1;
				currentTool = totalTools;
				lastTool = 0;
				windowActiveTool->hide();
				for (int i=0; i<=totalTools; i++)
				{
					pix.operator[](i)->show();
				}
				break;
			}

			// Distance limite de la main au capteur
			if (handPt.Z < (handDepthLimit + handDepthThreshold))
			{
				cursor.MoveEnable();
				cursor.ClicEnable();
				windowActiveTool->setBackgroundBrush(QBrush(Qt::green, Qt::SolidPattern));
			}
			else
			{
				cursor.MoveDisable();
				cursor.ClicDisable();
				windowActiveTool->setBackgroundBrush(QBrush(Qt::red, Qt::SolidPattern));
			}

			// Appel de la m�thode pour d�placer le curseur
			cursor.NewCursorVirtualPos((int)(handPt.X),(int)(handPt.Y),(int)(handPt.Z));
			break;
	}
}


void glutKeyboard (unsigned char key, int x, int y)
{
	float tmp = 0.0;
	switch (key)
	{

	// Exit
	case 27 :

		#ifdef _OS_WIN_
			ChangeCursor(0);
		#endif
		context.Shutdown();
		break;
	case 'i' :
		//increase smoothing
		tmp = g_fSmoothing + 0.1;
		g_fSmoothing = tmp;
		myHandsGenerator.SetSmoothing(g_fSmoothing);
		break;
	case 'o' :
		//decrease smoothing
		tmp = g_fSmoothing - 0.1;
		g_fSmoothing = tmp;
		myHandsGenerator.SetSmoothing(g_fSmoothing);
		break;
	case 's' :
		// Toggle smoothing
		if (g_fSmoothing == 0)
			g_fSmoothing = 0.1;
		else 
			g_fSmoothing = 0;
		myHandsGenerator.SetSmoothing(g_fSmoothing);
		break;
	case 'a' :
		//show some data for debugging purposes
		cout << "x= " << handPt.X << " ; y= " << handPt.Y << " ; z= " << handPt.Z << " ;   " << g_fSmoothing << " ;   " << currentState << endl;
		break;
	case 'y' :
		//show tools position
		for (int i=0; i<=totalTools; i++)
		{
			cout << "tool" << i << " : " << positionTool[i] << endl;
		}
		break;
	case 'e' :
		// end current session
		lastState = currentState;
		currentState = 0;
		sessionManager->EndSession();
		telnet.deconnexion();
		break;
	case 't' :
		methodeMainFermeeSwitch = !methodeMainFermeeSwitch;
		cout << "Switch Methode main fermee (" << (methodeMainFermeeSwitch?2:1) << ")" << endl;
		break;

		break;
	case '1' :
		RepositionnementFenetre(1);
		break;
	case '2' :
		RepositionnementFenetre(2);
		break;
	case '3' :
		RepositionnementFenetre(3);
		break;
	case '4' :
		RepositionnementFenetre(4);
		break;

	case '5' :

		// Enclenchement du timer
		glutTimerFunc(1000,TimerTest,12345);
				// Parametre1 : nombre de miliseconde
				// Parametre2 : nom de la fonction � appeler
				// Parametre3 : valeur pass�e en param�tre (p.ex num�ro de l'outil?)
		break;
	}
}


void onTimerOut(int value)
{
	timerOut = true;
}
void TimerTest(int value)
{
	cout << "\n TIMER : " << value << endl;
}


void glutDisplay()
{
	static unsigned compteurFrame = 0; compteurFrame++;

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	//clear the gl buffers
	status = context.WaitAnyUpdateAll();	//first update the context - refresh the depth/image data coming from the sensor
	
	// if the update failed, i.e. couldn't be read
	if(status != XN_STATUS_OK)
	{
		cout << "\nERROR:Read failed... Quitting!\n" << endl;	//print error message
		exit(0);	//exit the program
	}
	else
	{
		if(activeSession)
			sessionManager->Update(&context);
		dpGen.GetMetaData(dpMD);
		long xSize = dpMD.XRes();
		long ySize = dpMD.YRes();
		long totalSize = xSize * ySize;

		const XnDepthPixel*	depthMapData;
		depthMapData = dpMD.Data();

		int i, j, colorToSet;
		int depth;

		glLoadIdentity();
		glOrtho(0, xSize, ySize, 0, -1, 1);

		glBegin(GL_POINTS);
		for(i=0;i<xSize;i+=RES_WINDOW_GLUT)	// width
		{
			for(j=0;j<ySize;j+=RES_WINDOW_GLUT)	// height
			{
				depth = dpMD(i,j);
				colorToSet = MAX_COLOR - (depth/COLORS);

				if((depth < DP_FAR) && (depth > DP_CLOSE))
				{
					if (activeSession)
						glColor3ub(0,colorToSet,0);
					else
						glColor3ub(colorToSet,colorToSet,colorToSet);
					glVertex2i(i,j);
				}
			}
		}
		glEnd();	// End drawing sequence

		// Mise � jour de la detection de la main fermee
		if	( activeSession && (isHandPointNull() == false))
			UpdateHandClosed();

		//========== HAND POINT ==========//
		if	( activeSession && (isHandPointNull() == false)
				&& (hCD.ROI_Pt().x() >= 0)
				&& (hCD.ROI_Pt().y() >= 0)
				&& (hCD.ROI_Pt().x() <= (RES_X - hCD.ROI_Size().width()))
				&& (hCD.ROI_Pt().y() <= (RES_Y - hCD.ROI_Size().height())) )
		{
			glColor3f(255,255,255);	// Set the color to white
			glBegin(GL_QUADS);
			int size = 4;						// Size of the box
				glVertex2i(handPt.X-size,handPt.Y-size);
				glVertex2i(handPt.X+size,handPt.Y-size);
				glVertex2i(handPt.X+size,handPt.Y+size);
				glVertex2i(handPt.X-size,handPt.Y+size);
			glEnd();
		
			// Cadre de la main
			glColor3f(255,255,255);
			glBegin(GL_LINE_LOOP);
				glVertex2i(hCD.ROI_Pt().x(), hCD.ROI_Pt().y());
				glVertex2i(hCD.ROI_Pt().x()+hCD.ROI_Size().width(), hCD.ROI_Pt().y());
				glVertex2i(hCD.ROI_Pt().x()+hCD.ROI_Size().width(), hCD.ROI_Pt().y()+hCD.ROI_Size().height());
				glVertex2i(hCD.ROI_Pt().x(), hCD.ROI_Pt().y()+hCD.ROI_Size().height());
			glEnd();



			if (handStateChanged)
			{
				steadyState = false; sd.Reset();
				steady2 = false; sd2.Reset();
			}

			// mode Souris
			if ((currentState == 3) && (cursor.GetState() != 0))
			{
				// Souris SteadyClic
				if			(cursor.GetCursorType() == 1)
				{
					if (cursor.CheckExitMouseMode())
						cursor.ChangeState(0);
				}

				//Souris HandClosedClic
				else if ((cursor.GetCursorType() == 2) && (cursor.GetCursorInitialised()))
				{
					if (handFlancMont)
						cursor.SetMainFermee(true);
					else if (handFlancDesc)
						cursor.SetMainFermee(false);
				}
			}

			// Affichage des carr�s de couleurs pour indiquer l'etat de la main
			if (handClosed)
				glColor3ub(255,0,0);
			else
				glColor3ub(0,0,255);
			int cote = 50;
			int carreX = xSize-(cote+10), carreY = 10;
			glRecti(carreX,carreY,carreX+cote,carreY+cote);
			if (cursor.GetCursorType() == 2)
				glRecti(carreX,carreY+10+cote,carreX+cote,carreY+10+2*cote);

		}
	}
	glutSwapBuffers();

	if (actualFrame >= nFrames)
	{
		handleState();
		actualFrame = -1;
	}
	else if(actualFrame == 0)
	{
		lastX = handPt.X;
		lastY = handPt.Y;
		lastZ = handPt.Z;
	}
	actualFrame = actualFrame+1;
}


void UpdateHandClosed(void)
{
	if (toolSelectable)
	{
		hCD.Update((methodeMainFermeeSwitch?2:1),dpMD,handPt);
		handStateChanged = hCD.HandClosedStateChanged();
		handFlancMont = hCD.HandClosedFlancMont();
		handFlancDesc = hCD.HandClosedFlancDesc();
		handClosed = hCD.HandClosed();
		handClic = hCD.HandClosedClic(9);

		//// Controle de detection de la main fermee //
		//if (handStateChanged)
		//	cout << endl << "\t\tChgmt d'etat de la main!" << endl;
		//if (handFlancMont)
		//	cout << endl << "\t\tFermeture de la main!" << endl;
		//if (handFlancDesc)
		//	cout << endl << "\t\tOuverture de la main!" << endl;
		////if (handClosed)
		////	cout << endl << "\t\tMain Fermee!" << endl;
		////else
		////	cout << endl << "\t\tMain Ouverte!" << endl;
		//if (handClic)
		//	cout << endl << "\t\tClic de la main!" << endl;
	}
}


void initGL(int argc, char *argv[])
{
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(INIT_WIDTH_WINDOW, INIT_HEIGHT_WINDOW);

	// Fen�tre de donn�es source
	glutCreateWindow(TITLE);
	RepositionnementFenetre(INIT_POS_WINDOW);
	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);

	// Idle callback (pour toutes les fen�tres)
	glutIdleFunc(glutDisplay);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
}


//==========================================================================//
//================================= MAIN ===================================//

int main(int argc, char *argv[])
{

	Initialisation();

	/////////////////////////////////////////// OPEN_NI / NITE / OPENGL ////////////////
	xn::EnumerationErrors errors;

	status = context.InitFromXmlFile(XML_FILE);
	CHECK_ERRORS(status, errors, "InitFromXmlFile");
	CHECK_STATUS(status, "InitFromXml");

	//si le context a �t� initialis� correctement
	status = context.FindExistingNode(XN_NODE_TYPE_DEPTH, dpGen);
	CHECK_STATUS(status, "Find depth generator");
	status = context.FindExistingNode(XN_NODE_TYPE_HANDS, myHandsGenerator);
	CHECK_STATUS(status, "Find hands generator");

	// NITE 
	sessionManager = new XnVSessionManager();

	//Focus avec un coucou et Refocus avec "RaiseHand" 
	status = sessionManager->Initialize(&context,"Wave","Wave,RaiseHand");
	CHECK_STATUS(status,"Session manager");

	sessionManager->RegisterSession(&context,sessionStart,sessionEnd, FocusProgress);
	sessionManager->SetQuickRefocusTimeout(5000);

	g_pFlowRouter = new XnVFlowRouter;

	pointControl = new XnVPointControl("Point Tracker");
	pointControl->RegisterPrimaryPointCreate(&context,pointCreate);
	pointControl->RegisterPrimaryPointDestroy(&context,pointDestroy);
	pointControl->RegisterPrimaryPointUpdate(&context,pointUpdate);

//======================= STEADY ================================//

	// Steady
	sd.RegisterSteady(NULL,&Steady_Detected);
	sd.RegisterNotSteady(NULL,&NotSteady_Detected);
	sd.SetDetectionDuration(1200);
	sd.SetMaximumStdDevForSteady(MAX_STD_DEV_FOR_STEADY);
	sd.SetMinimumStdDevForNotSteady(MAX_STD_DEV_FOR_NOT_STEADY);

	// Steady 2
	sd2.RegisterSteady(NULL,&Steady_Detected2);
	sd2.RegisterNotSteady(NULL,&NotSteady_Detected2);
	sd2.SetDetectionDuration(2000);
	sd2.SetMaximumStdDevForSteady(MAX_STD_DEV_FOR_STEADY);
	sd2.SetMinimumStdDevForNotSteady(MAX_STD_DEV_FOR_NOT_STEADY);

	// Steady 3
	sd3.RegisterSteady(NULL,&Steady_Detected3);
	sd3.SetDetectionDuration(3000);
	sd3.SetMaximumStdDevForSteady(MAX_STD_DEV_FOR_STEADY);
	sd3.SetMinimumStdDevForNotSteady(MAX_STD_DEV_FOR_NOT_STEADY);

		// Steady 02
	sd02.RegisterSteady(NULL,&Steady_Detected02);
	sd02.SetDetectionDuration(200);
	sd02.SetMaximumStdDevForSteady(MAX_STD_DEV_FOR_STEADY);
	sd02.SetMinimumStdDevForNotSteady(MAX_STD_DEV_FOR_NOT_STEADY);

	// Wave detector
	XnVWaveDetector waveDetect;
	waveDetect.RegisterWave(&context,&Wave_Detected);
	//waveDetect.SetFlipCount(10);
	//waveDetect.SetMaxDeviation(1);
	//waveDetect.SetMinLength(100);

	// Add Listener
	sessionManager->AddListener(pointControl);
	sessionManager->AddListener(&sd);
	sessionManager->AddListener(&sd2);
	sessionManager->AddListener(&sd3);
	sessionManager->AddListener(&sd02);
	sessionManager->AddListener(g_pFlowRouter);
	sessionManager->AddListener(&waveDetect);
		
	nullifyHandPoint();
	myHandsGenerator.SetSmoothing(g_fSmoothing);

	// Initialization done. Start generating
	status = context.StartGeneratingAll();
	CHECK_STATUS(status, "StartGenerating");

	initGL(argc,argv);


	//Qt
#ifdef _OS_MAC_
	int qargc = 0;
	char **qargv = NULL;
	QApplication app(qargc,qargv);
#endif
	window = new GraphicsView(NULL);
	windowActiveTool = new GraphicsView(NULL);
	sceneActiveTool = new QGraphicsScene(0,0,128,128);
	viewLayouts = new GraphicsView(NULL);
	pixActive = new Pixmap(QPixmap()); //for activeTool




	//================== QT ===================//

	// Initialisation des ressources et cr�ation de la fen�tre avec les ic�nes
	Q_INIT_RESOURCE(images);

#if defined _OS_WIN_
	Pixmap *p1 = new Pixmap(QPixmap(":/images/Resources/mouse.png").scaled(64,64));
	Pixmap *p2 = new Pixmap(QPixmap(":/images/Resources/layout.png").scaled(64,64));
	Pixmap *p3 = new Pixmap(QPixmap(":/images/Resources/move.png").scaled(64,64));
	Pixmap *p4 = new Pixmap(QPixmap(":/images/Resources/zoom.png").scaled(64,64));
	Pixmap *p5 = new Pixmap(QPixmap(":/images/Resources/scroll.png").scaled(64,64));
	Pixmap *p6 = new Pixmap(QPixmap(":/images/Resources/contrast.png").scaled(64,64));
	Pixmap *p7 = new Pixmap(QPixmap(":/images/Resources/stop.png").scaled(64,64));
#elif defined _OS_MAC_
	Pixmap *p1 = new Pixmap(QPixmap(":/images/res/mouse.png").scaled(64,64));
	Pixmap *p2 = new Pixmap(QPixmap(":/images/res/layout.png").scaled(64,64));
	Pixmap *p3 = new Pixmap(QPixmap(":/images/res/move.png").scaled(64,64));
	Pixmap *p4 = new Pixmap(QPixmap(":/images/res/zoom.png").scaled(64,64));
	Pixmap *p5 = new Pixmap(QPixmap(":/images/res/scroll.png").scaled(64,64));
	Pixmap *p6 = new Pixmap(QPixmap(":/images/res/contrast.png").scaled(64,64));
	Pixmap *p7 = new Pixmap(QPixmap(":/images/res/stop.png").scaled(64,64));
#endif

	p1->setObjectName("mouse");
	p2->setObjectName("layout");
	p3->setObjectName("move");
	p4->setObjectName("zoom");
	p5->setObjectName("scroll");
	p6->setObjectName("contrast");
	p7->setObjectName("stop");

	p1->setGeometry(QRectF(  0.0,   192.0, 64.0, 64.0));
	p2->setGeometry(QRectF(  128.0,   192.0, 64.0, 64.0));
	p3->setGeometry(QRectF(  256.0,   192.0, 64.0, 64.0));
	p4->setGeometry(QRectF(  384.0,   192.0, 64.0, 64.0));
	p5->setGeometry(QRectF(  512.0,   192.0, 64.0, 64.0));
	p6->setGeometry(QRectF(  640.0,   192.0, 64.0, 64.0));
	p7->setGeometry(QRectF(  768.0,   192.0, 64.0, 64.0));

	pix.push_back(p1);
	pix.push_back(p2);
	pix.push_back(p3);
	pix.push_back(p4);
	pix.push_back(p5);
	pix.push_back(p6);
	pix.push_back(p7);

	//window->setSize(1024,288);
	window->setSize(896,256);
	
	//window->setSize(548,window->getResY()-100);
	QGraphicsScene *scene = new QGraphicsScene(0,0,896,256);
	//QGraphicsScene *scene = new QGraphicsScene(0,(-window->getResY())+488,548,window->getResY()-100);
	scene->addItem(p1);
	scene->addItem(p2);
	scene->addItem(p3);
	scene->addItem(p4);
	scene->addItem(p5);
	scene->addItem(p6);
	scene->addItem(p7);
	window->setScene(scene);

	sceneActiveTool->addItem(pixActive);
	//windowActiveTool->setSize(126,126);
	windowActiveTool->setScene(sceneActiveTool);
	windowActiveTool->setGeometry(window->getResX()-128,window->getResY()-168,128,128);

	//window->show();
	/////////////////////////////////////////////////////////////////////////////////////



	////////////// LAYOUT
#if defined _OS_WIN_
	Pixmap *l1 = new Pixmap(QPixmap(":/images/Resources/layouts/_1x1.png").scaled(64,64));
	Pixmap *l2 = new Pixmap(QPixmap(":/images/Resources/layouts/_1x2.png").scaled(64,64));
	Pixmap *l3 = new Pixmap(QPixmap(":/images/Resources/layouts/_2x1.png").scaled(64,64));
	Pixmap *l4 = new Pixmap(QPixmap(":/images/Resources/layouts/_3a.png").scaled(64,64));
	Pixmap *l5 = new Pixmap(QPixmap(":/images/Resources/layouts/_3b.png").scaled(64,64));
	Pixmap *l6 = new Pixmap(QPixmap(":/images/Resources/layouts/_2x2.png").scaled(64,64));
#elif defined _OS_MAC_
	Pixmap *l1 = new Pixmap(QPixmap(":/images/res/layouts/_1x1.png").scaled(64,64));
	Pixmap *l2 = new Pixmap(QPixmap(":/images/res/layouts/_1x2.png").scaled(64,64));
	Pixmap *l3 = new Pixmap(QPixmap(":/images/res/layouts/_2x1.png").scaled(64,64));
	Pixmap *l4 = new Pixmap(QPixmap(":/images/res/layouts/_3a.png").scaled(64,64));
	Pixmap *l5 = new Pixmap(QPixmap(":/images/res/layouts/_3b.png").scaled(64,64));
	Pixmap *l6 = new Pixmap(QPixmap(":/images/res/layouts/_2x2.png").scaled(64,64));
#endif

	l1->setObjectName("1x1");
	l2->setObjectName("1x2");
	l3->setObjectName("2x1");
	l4->setObjectName("3a");
	l5->setObjectName("3b");
	l6->setObjectName("2x2");
	
	l1->setGeometry(QRectF(0.0,   192.0, 64.0, 64.0));
	l2->setGeometry(QRectF(128.0, 192.0, 64.0, 64.0));
	l3->setGeometry(QRectF(256.0, 192.0, 64.0, 64.0));
	l4->setGeometry(QRectF(384.0, 192.0, 64.0, 64.0));
	l5->setGeometry(QRectF(512.0, 192.0, 64.0, 64.0));
	l6->setGeometry(QRectF(640.0, 192.0, 64.0, 64.0));

	pixL.push_back(l1);
	pixL.push_back(l2);
	pixL.push_back(l3);
	pixL.push_back(l4);
	pixL.push_back(l5);
	pixL.push_back(l6);

	viewLayouts->setSize(768,256);
	QGraphicsScene *sceneLayout = new QGraphicsScene(0,0,768,256);
	sceneLayout->addItem(l1);
	sceneLayout->addItem(l2);
	sceneLayout->addItem(l3);
	sceneLayout->addItem(l4);
	sceneLayout->addItem(l5);
	sceneLayout->addItem(l6);
	viewLayouts->setScene(sceneLayout);

	//viewLayouts->show();

	
	/*for(int i=0; i<=totalTools; i++){
		QString chemin = ":/images/Resources/_"+pix.operator[](i)->objectName()+".png";
		//printf("\n"+chemin.toAscii()+"\n");
		int posi = positionTool[i];
		pix.operator[](i)->setGeometry(QRectF( posi*60.0, posi*(-10.0), 128.0, 128.0));
		pix.operator[](i)->load(QPixmap(chemin).scaled(78+(posi*(10)),78+(posi*(10))));
	}*/

	// Boucle principale
	glutMainLoop();
	
	return app.exec();
}



/**** CALLBACK DEFINITIONS ****/

/**********************************************************************************
Session started event handler. Session manager calls this when the session begins
**********************************************************************************/
void XN_CALLBACK_TYPE sessionStart(const XnPoint3D& ptPosition, void* UserCxt)
{
	activeSession = true;
	cout << "Debut de la session" << endl;
	window->show();
	lastState = currentState;
	currentState = 1;
	toolSelectable = false;
	steadyState = false;
	steady2 = false;
	hCD.ResetCompteurFrame();
}

/**********************************************************************************
session end event handler. Session manager calls this when session ends
**********************************************************************************/
void XN_CALLBACK_TYPE sessionEnd(void* UserCxt)
{
	activeSession = false;
	cout << "Fin de la session" << endl;
	window->hide();
	viewLayouts->hide();
	lastState = currentState;
	currentState = 0;
}

/**********************************************************************************
point created event handler. this is called when the pointControl detects the creation
of the hand point. This is called only once when the hand point is detected
**********************************************************************************/
void XN_CALLBACK_TYPE pointCreate(const XnVHandPointContext *pContext, const XnPoint3D &ptFocus, void *cxt)
{
	XnPoint3D coords(pContext->ptPosition);
	dpGen.ConvertRealWorldToProjective(1,&coords,&handPt);
	lastPt = handPt;
}
/**********************************************************************************
Following the point created method, any update in the hand point coordinates are 
reflected through this event handler
**********************************************************************************/
void XN_CALLBACK_TYPE pointUpdate(const XnVHandPointContext *pContext, void *cxt)
{
	XnPoint3D coords(pContext->ptPosition);
	dpGen.ConvertRealWorldToProjective(1,&coords,&handPt);
}
/**********************************************************************************
when the point can no longer be tracked, this event handler is invoked. Here we 
nullify the hand point variable 
**********************************************************************************/
void XN_CALLBACK_TYPE pointDestroy(XnUInt32 nID, void *cxt)
{
	lastState = currentState;
	currentState = 0;
	windowActiveTool->hide();
	for (int i=0; i<=totalTools; i++)
	{
		pix.operator[](i)->show();
	}
	window->hide();
	nullifyHandPoint();
	cout << "Point detruit" << endl;
}


// Callback for no hand detected
void XN_CALLBACK_TYPE NoHands(void* UserCxt)
{
	cursor.ChangeState(0);
}

// Callback for when the focus is in progress
void XN_CALLBACK_TYPE FocusProgress(const XnChar* strFocus, 
		const XnPoint3D& ptPosition, XnFloat fProgress, void* UserCxt)
{
	//cout << "Focus progress: " << strFocus << " @(" << ptPosition.X << "," 
	//			<< ptPosition.Y << "," << ptPosition.Z << "): " << fProgress << "\n" << endl;

	/// Pour r�afficher l'�cran s'il s'est �teint
	POINT temp;
	GetCursorPos(&temp);
	int test = ((temp.x < SCRSZW/2) ? 1 : -1);
	for (int i=1; i<50; i++)
		SetCursorPos(temp.x + i*test, temp.y);
	//SetCursorPos(temp.x, temp.y);
	//cout << "AAAAAAAAA" << endl;
}


// Callback for wave
void XN_CALLBACK_TYPE Wave_Detected(void *pUserCxt)
{
	cout << "\n WAVE \n";
}

// Callback for steady
void XN_CALLBACK_TYPE Steady_Detected(XnUInt32 nId, XnFloat fStdDev, void *pUserCxt)
{
	cout << "  STEADY 1\n";
	if (currentState != 4)
	{
		// Mode souris
		if (currentState == 3)
		{
			cursor.SteadyDetected(1);
			//sd.Reset();
		}
		else
			steadyState = true;
	}
}

void XN_CALLBACK_TYPE Steady_Detected2(XnUInt32 nId, XnFloat fStdDev, void *pUserCxt)
{
	cout << "  STEADY 2\n";
	// Mode souris
	if (currentState == 3)
		cursor.SteadyDetected(2);

	// Autres outils
	else if (currentState == 2)
		steady2 = true;
}

void XN_CALLBACK_TYPE Steady_Detected3(XnUInt32 nId, XnFloat fStdDev, void *pUserCxt)
{
	cout << "  STEADY 3\n";
	// Mode souris
	if (currentState == 3)
		cursor.SteadyDetected(3);
}

void XN_CALLBACK_TYPE Steady_Detected02(XnUInt32 nId, XnFloat fStdDev, void *pUserCxt)
{
	cout << "  STEADY 02\n";
	toolSelectable = true;
}

// Callback for not steady
void XN_CALLBACK_TYPE NotSteady_Detected(XnUInt32 nId, XnFloat fStdDev, void *pUserCxt)
{
	cout << "\n NOT STEADY \n";
	steadyState = false;

	// Mode souris
	if (currentState == 3)
		cursor.NotSteadyDetected();
}

void XN_CALLBACK_TYPE NotSteady_Detected2(XnUInt32 nId, XnFloat fStdDev, void *pUserCxt)
{
	steady2 = false;
}





