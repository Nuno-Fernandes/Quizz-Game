#include <iostream> // cout
#include <stdlib.h>  // exit
#include <string.h> // bzero
#include <string>
#include <sstream>  //cout
#include <unistd.h> // close
#include <pthread.h> //threads
#include <set> //set global clientes
#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <postgresql/libpq-fe.h>
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

pthread_mutex_t mutex;

set<int> clientes;

PGconn* conn = NULL;

map<string, int> sockets;
map<int, string> usernames;

int counter=0;

void initDB()
{
  conn = PQconnectdb("host='vdbm.fe.up.pt' user='HIDDEN' password='HIDDEN' dbname='HIDDEN'");
 
  if (!conn)
  {
    cout << "Não foi possivel ligar a base de dados" << endl;
    exit(-1);
  }
 
  if (PQstatus(conn) != CONNECTION_OK)
  {
    cout << "Não foi possivel ligar a base de dados" << endl;
    exit(-1);
  }
}
 
PGresult* executeSQL(string sql)
{
  cout << "SQL: " << sql << endl;
  PGresult* res = PQexec(conn, sql.c_str());
 
  if (!(PQresultStatus(res) == PGRES_COMMAND_OK || PQresultStatus(res) == PGRES_TUPLES_OK))
  {
    cout << "Não foi possivel executar o comando!" << endl;
    return NULL;
  }
 
  return res;
}
 
void closeDB()
{
  PQfinish(conn);
}

int StringToInt(string i)
{
    istringstream buffer(i);
    int value;
    buffer>>value;
    return value;
}


string IntToString(int i)
{
    ostringstream oss;
    oss << i;
    return oss.str();
}

void writeline(int socketfd, string line) 
{   

    string tosend = "Server: "+line + "\n";
    write(socketfd, tosend.c_str(), tosend.length());
}  

void say(int de,int para, string line)
{
    string tosend = usernames[de]+": "+line+"\n";   
    write(para, tosend.c_str(), tosend.length());
}



bool readline(int socketfd, string &line) 
{
  int n; 
  /* buffer de tamanho 1025 para ter espaço 
     para o \0 que indica o fim de string*/
  char buffer[1025]; 

  /* inicializar a string */
  line = "";

  /* Enquanto não encontrarmos o fim de linha
     vamos lendo mais dados da stream */
  while (line.find('\n') == string::npos) 
  {
    // leu n carateres. se for zero chegamos ao fim
    int n = read(socketfd, buffer, 1024);
    if (n == 0) return false;
    buffer[n] = 0; // colocar o \0 no fim do buffer
    line += buffer; // acrescentar os dados lidos à string
  }

  // Retirar o \r\n
  line.erase(line.end() - 1);
  line.erase(line.end() - 1);
  return true;  
}

/*
void broadcast(int origin, string text)
{
    ostringstream message;
    message<<origin<<": "<<text;
    
    pthread_mutex_lock(&mutex);
    set<int>::iterator it;
    for (it = clientes.begin(); it != clientes.end(); it++)
        {
        if (*it != origin)
            writeline(*it, message.str());
        }
    pthread_mutex_unlock(&mutex);
}
*/

vector<string> split(const string &s, char delim) {
    vector<string> elems;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


void* Jogo(void* pos)
{
	ostringstream oss_jogo, oss_periodo, oss_nper, oss_idper;
	int id_jogo = *(int*)pos;
	int num_perguntas, tempo_resposta, id_question;
	string flag_pos;
	string jogadores, question, ans1, ans2, ans3, ans4, id_jogo_str;
	id_jogo_str = IntToString(id_jogo);
	srand(time(NULL));
	
	pthread_mutex_lock(&mutex);
	

	PGresult* res = executeSQL("SELECT username FROM desafia WHERE aceita='true' AND id_jogo='"+id_jogo_str+"'");
	for (int row = 0; row < PQntuples(res); row++){
		oss_jogo<<PQgetvalue(res,row,0)<<"|";
	}
	jogadores = oss_jogo.str();
	vector<string> gamer = split(jogadores, '|');
	
	PGresult* res2 = executeSQL("SELECT periodo, numperguntas FROM jogo WHERE id='"+id_jogo_str+"'");
	oss_periodo<<PQgetvalue(res2,0,0)<<endl;
	tempo_resposta=StringToInt(oss_periodo.str());
	
	oss_nper<< PQgetvalue(res2,0,1)<<endl;
	num_perguntas = StringToInt(oss_nper.str());
	
	while(num_perguntas>0)
	{
		PGresult* res3 = executeSQL("SELECT * FROM pergunta ORDER BY random() LIMIT 1");  
		oss_idper<<PQgetvalue(res3,0,0)<<endl;
		id_question = StringToInt(oss_idper.str());
		question = PQgetvalue(res3,0,1);
		ans1 = PQgetvalue(res3,0,2);
		ans2 = PQgetvalue(res3,0,3);
		ans3 = PQgetvalue(res3,0,4);
		ans4 = PQgetvalue(res3,0,5);
		string totalans[4] = {ans1,ans2,ans3,ans4};
		random_shuffle(totalans, totalans + 4);
		
		if(totalans[0]==ans1)
			flag_pos="a";
		else if(totalans[1]==ans1)
			flag_pos="b";
		else if(totalans[2]==ans1)
			flag_pos="c";	
		else if(totalans[3]==ans1)
			flag_pos="d";	
		else
			cout<<"ERRO FATAL!"<<endl;
		
		executeSQL("INSERT INTO ronda VALUES(default, '"+flag_pos+"', '"+id_jogo_str+"', '"+IntToString(id_question)+"')");
			
		for(int i=0; i<gamer.size(); i++)
		{	
			writeline(sockets[gamer[i]], ""+question+"  a-"+totalans[0]+" b-"+totalans[1]+" c-"+totalans[2]+" d-"+totalans[3]+"");
		}
		pthread_mutex_unlock(&mutex);
		usleep(1000000*tempo_resposta);
		
		num_perguntas=num_perguntas-1;
	}
		for(int o=0; o<gamer.size(); o++)
		{
		executeSQL("UPDATE jogador SET estado='false' WHERE username='"+gamer[o]+"'");
		writeline(sockets[gamer[o]],"O jogo terminou.");
		}
		executeSQL("UPDATE jogo SET estado='Terminado' WHERE id='"+id_jogo_str+"'");
			
		counter=0;
		
        	
				
}







void* cliente(void* args)
{
    

    string comando, segundo, terceiro, nomes, nomes2, nomes3;
    int sockfd = *(int*)args, pos;
    string line;
    
    clientes.insert(sockfd);    
    
    cout<<"Reading from Socket"<<sockfd<<endl;

    while (readline(sockfd,line))
    {
    if (line.find("\\")==0)
        {        
        ostringstream oss;
        
        istringstream iss(line);
        iss>>comando>>segundo;
        

        string palavra;
        while (iss>>palavra)
            terceiro=terceiro+palavra+" ";
        
               
        if ((comando.find("\\register")==0))
        {
            pthread_mutex_lock(&mutex);
            
            //initDB();
             PGresult* res = executeSQL("SELECT * FROM jogador");
             for (int row = 0; row < PQntuples(res); row++)
             {
                //cout << PQgetvalue(res,row,0) << endl;
                oss<<PQgetvalue(res,row,0)<<" ";
             }
            nomes=oss.str();
            
                    
            if (nomes.find(segundo) != string::npos)
                writeline(sockfd,"Esse username ja existe");
            
            else
            {
                executeSQL("INSERT INTO jogador VALUES('"+segundo+"', '"+terceiro+"', false, 1, 0)");
                writeline(sockfd,"Bem vindo!");
            }
            
            pthread_mutex_unlock(&mutex);
            
            comando=segundo=terceiro=nomes=nomes2="";
                     
        }       
        
        else if (comando.find("\\say")==0)
            {
            pthread_mutex_lock(&mutex);
            
            if (usernames.find(sockfd) == usernames.end())
                writeline(sockfd, "Precisa de estar logged in para executar este comando.");
            
            else if ((sockets.find(segundo) != sockets.end()) && (terceiro.compare("") != 0))
                {
                pos=sockets[segundo];
                say(sockfd,pos,terceiro);
                comando=segundo=terceiro=nomes=nomes2="";
                }
            else if ((segundo.compare("")!=0) && (sockets.find(segundo) == sockets.end()))
                writeline(sockfd, "Esse username não existe ou não se encontra online");
                                    
            else
                writeline(sockfd, "O comando foi mal executado.");
            
            comando=segundo=terceiro=nomes=nomes2="";
            pthread_mutex_unlock(&mutex);
            }
        
        else if ((comando.find("\\login")==0) )
            {
            pthread_mutex_lock(&mutex);
            
                       
            PGresult* res = executeSQL("SELECT * FROM jogador");
            for (int row = 0; row < PQntuples(res); row++)
                oss << PQgetvalue(res,row,0) << " " << PQgetvalue(res,row,1) << endl;
            
                        
            nomes=oss.str();
            cout<<nomes<<endl;
            if (sockets.find(segundo) != sockets.end())
                writeline(sockfd, "O utilizador ja está online, introduza outro username ou registe-se com \\register");
            
            else if (segundo.compare("")==0)
                writeline(sockfd, "O comando foi mal executado.");
            
            else if (nomes.find(segundo) == string::npos)
                writeline(sockfd, "O username não existe na base de dados");
            
            else if (nomes.find(segundo)!=string::npos)
            {               
                                
                PGresult* res = executeSQL("SELECT password FROM jogador WHERE username='"+segundo+"'");
                if (PQgetvalue(res, 0, 0) == terceiro)
                {
                    counter=1;
                    sockets[segundo] = sockfd;
                    usernames[sockfd] = segundo;
                    writeline(sockfd, "Fez login com sucesso!");
                }  
                
                else
                    writeline(sockfd, "A password está errada.");
            }
            
                
        pthread_mutex_unlock(&mutex);
        comando=segundo=terceiro=nomes=nomes2="";
            }
        else if(comando.find("\\info")==0)
            {
            pthread_mutex_lock(&mutex);
            if ((segundo.compare("")==0) && (usernames.find(sockfd) != usernames.end()))
                {
                nomes=usernames[sockfd];
                PGresult* res = executeSQL("SELECT * FROM jogador WHERE username='"+nomes+"'");                
                writeline(sockfd,"O seu username é "+usernames[sockfd]+", o seu privilégio é "+PQgetvalue(res,0,3)+" (de 0 a 2)e o seu ranking é "+PQgetvalue(res,0,4));   
                }
            else if((segundo.compare("")!=0) && (usernames.find(sockfd) != usernames.end()))
                {
                PGresult* res = executeSQL("SELECT * FROM jogador WHERE username='"+segundo+"'");
                oss<<PQgetvalue(res,0,0);
                nomes=oss.str();
                if (nomes == "")
                    {
                    writeline(sockfd,"Esse username não existe!");
                    }
                else
                    {
                    oss<<PQgetvalue(res,0,2);
                    nomes=oss.str();
                    if (nomes.compare("f")==0)
                        nomes="não está a jogar";
                    else if (nomes.compare("t")==0)
                        nomes="está a jogar";
                    
                    nomes2=PQgetvalue(res,0,0);

                    if (sockets.find(nomes2) != sockets.end())
                        terceiro=" (online)";
                    else if (sockets.find(nomes2) == sockets.end())
                        terceiro=" (offline)";
                    
                    writeline(sockfd,"O jogador é "+nomes2+terceiro+", "+nomes+", o seu privilégio é "+PQgetvalue(res,0,3)+" (de 0 a 2) e o seu ranking é "+PQgetvalue(res,0,4));
                    }
                }
            else
                writeline(sockfd,"Precisa de estar logged in para executar este comando.");
            pthread_mutex_unlock(&mutex);
            comando=segundo=terceiro=nomes=nomes2="";
            }
        
        else if (comando.find("\\ranking")==0)
            {   
                if (usernames.find(sockfd) == usernames.end())
                    writeline(sockfd,"Precisa de estar logged in para executar este comando.");
                
                else if ((segundo.compare("")!=0) || (terceiro.compare("")!=0))
                    writeline(sockfd,"O comando foi mal executado.");
            
                else
                    {
                    PGresult* res = executeSQL("SELECT * FROM jogador ");
                    for (int row = 0; row < PQntuples(res); row++)
                        oss << PQgetvalue(res,row,0) << " - ranking: " << PQgetvalue(res,row,4) << endl;
                    nomes=oss.str();
                    writeline(sockfd, "\n"+nomes);    
                    }   
                                
            
            comando=segundo=terceiro=nomes=nomes2="";
            }
        
        else if (comando.find("\\exit")==0)
            {
            if (usernames.find(sockfd) != usernames.end())
                {
                                
                writeline(sockfd, "Adeus!");
                counter=0;
                nomes = usernames[sockfd];
                sockets.erase(nomes);
                usernames.erase(sockfd);
                                                
                comando=segundo=terceiro=nomes=nomes2="";
                }
            else
                writeline(sockfd, "Precisa de estar logged in para executar este comando."); 
        
            }
        /*else if(comando.find("\\identify")==0)
            {
             
            PGresult* res = executeSQL("SELECT * FROM jogador");
            for (int row = 0; row < PQntuples(res); row++)
                oss << PQgetvalue(res,row,0) << " " << PQgetvalue(res,row,1) << endl;
            
            nomes=oss.str();
            
            if (nomes.find(segundo)!=string::npos)
                writeline(sockfd, "O username que tentou introduzir ja está a ser utilizado.");
            
            else if(nomes.find(segundo)==string::npos)
                {
                counter=2;
                guest=segundo;
                executeSQL("INSERT INTO jogador VALUES('"+segundo+"', 'joaquim', false, 1, 0)");    //ta na base de dados
                sockets[segundo] = sockfd;     //ta online
                usernames[sockfd] = segundo;
                writeline(sockfd, "Fez login como convidado com sucesso! Para poder criar jogos e adicionar perguntas registe-se com \\register.");
                }
            
            else if(segundo=="")
                {
                counter=2;
                segundo="guest"+IntToString(sockfd);
                guest=segundo;
                executeSQL("INSERT INTO jogador VALUES('"+segundo+"', 'joaquim', false, 1, 0)");
                sockets[segundo] = sockfd;     //ta online
                usernames[sockfd] = segundo;
                writeline(sockfd, "Fez login como convidado ("+segundo+") com sucesso! Para poder criar jogos e adicionar perguntas registe-se com \\register.");
                }
                comando=segundo=terceiro=nomes=nomes2="";
            }*/
        
        else if ((comando.find("\\question")==0) && ((segundo.compare("")!=0) || (terceiro.compare("")!=0)))        
            {
            pthread_mutex_lock(&mutex);
            
            if (usernames.find(sockfd) != usernames.end())
                {
                
                nomes = segundo +" "+ terceiro;
                nomes2=usernames[sockfd];
                vector<string> items = split(nomes, '|');
                if (items.size()!=5)
                    writeline(sockfd,"O comando foi mal executado");
                else 
                    executeSQL("INSERT INTO pergunta VALUES(default, '"+items[0]+"', '"+items[1]+"', '"+items[2]+"', '"+items[3]+"', '"+items[4]+"', '"+nomes2+"')");
                }
            else
                writeline(sockfd,"Precisa de estar logged in para executar este comando.");
            pthread_mutex_unlock(&mutex);
            comando=segundo=terceiro=nomes=nomes2="";
            }
        
      
      
      
        
        else if((comando.find("\\create")==0) && ((segundo.compare("")!=0) || (terceiro.compare("")!=0)) && (counter!=2))  //SO FUNCIONA COM NUMEROS
            {
            pthread_mutex_lock(&mutex); 
            
            PGresult* res2 = executeSQL("SELECT id FROM jogo WHERE username = '"+usernames[sockfd]+"' AND estado = 'Pendente'");
            oss<<PQgetvalue(res2,0,0);
            nomes3=oss.str();
            if (nomes3.compare("") == 0)
                {
                
            
                if (usernames.find(sockfd) != usernames.end())
                {   
                    counter=2;            
                    nomes=usernames[sockfd];
                    
                    executeSQL("INSERT INTO jogo VALUES(default, '"+segundo+"', '"+terceiro+"', 'Pendente', '"+nomes+"')");
                    PGresult* res = executeSQL("SELECT id FROM jogo WHERE username = '"+usernames[sockfd]+"' AND estado = 'Pendente'");
                    nomes2=PQgetvalue(res,0,0);
                    executeSQL("INSERT INTO desafia VALUES(default,'"+usernames[sockfd]+"', true,"+nomes2+" )");
                    writeline(sockfd, "O seu jogo foi criado com sucesso.");
                }
                else    
                    writeline(sockfd,"Precisa de estar logged in para executar este comando.");
                pthread_mutex_unlock(&mutex);
                comando=segundo=terceiro=nomes=nomes2="";
                }
            else
                writeline(sockfd, "Já tem um jogo pendente");
            
            comando=segundo=terceiro=nomes=nomes2=nomes3="";
            }
       
              
       
               
        else if((comando.find("\\challenge")==0) && (segundo!="")) //mudar de forma a deixar convidar o mesmo jogador so um vez
            {
                pthread_mutex_lock(&mutex); 
                if (usernames.find(sockfd) != usernames.end())
                {
                    PGresult* res = executeSQL("SELECT username FROM jogo");
                    for (int row = 0; row < PQntuples(res); row++)
                        oss << PQgetvalue(res,row,0) << endl;
                    nomes=oss.str();
                    if(nomes.find(usernames[sockfd])!=string::npos) //se o jogador que deu challenge tem um jogo criado
                        {   
							 PGresult* res3 = executeSQL("SELECT username FROM jogador");
							for (int row = 0; row < PQntuples(res3); row++)
								oss << PQgetvalue(res3,row,0) << " " <<endl;
							nomes3 = oss.str();
							if (nomes3.find(segundo) == string::npos)
								writeline(sockfd, "Esse jogador não existe");
							else
								{
                            //oss.str()="";
                            pos=sockets[segundo];
                            PGresult* res2 = executeSQL("SELECT id FROM jogo WHERE username='"+usernames[sockfd]+"' AND estado='Pendente'");
                            nomes2=PQgetvalue(res2,0,0);                                //saca o id do jogo
                            cout<<nomes2<<endl;
                            cout<<oss.str()<<endl;
                            executeSQL("INSERT INTO desafia VALUES(default,'"+segundo+"',false,'"+nomes2+"')");
                            say(sockfd, pos, "Foi desafiado para um jogo por "+usernames[sockfd]+"! Use o comando \\accept para aceitar.");
                            writeline(sockfd, "foi enviado um desafio ao jogador "+segundo+".");
								}
                        }
                    else    
                        writeline(sockfd,"Precisa de criar um jogo antes de desafiar outro jogador!");
                        
                }
                else    
                    writeline(sockfd,"Precisa de estar logged in para executar este comando.");
                pthread_mutex_unlock(&mutex);
                comando=segundo=terceiro=nomes=nomes2=nomes3="";
            }
		
		else if (comando.find("\\accept")==0)
			{
			pthread_mutex_lock(&mutex);
			
			if (usernames.find(sockfd) != usernames.end())		//ver se ta logged in
				{
				PGresult* res = executeSQL("SELECT * FROM desafia WHERE username='"+usernames[sockfd]+"'");
				for (int row = 0; row < PQntuples(res); row++)
					oss<<PQgetvalue(res,row,3)<<" ";		// buscar todos os id's dos jogos onde ele ta desafiado
				nomes=oss.str();
				cout<<nomes<<endl;
				if (segundo.compare("") != 0)
					{
					//oss.str("");
					//oss.clear();
					PGresult* res2 = executeSQL("SELECT id FROM jogo WHERE username='"+segundo+"' AND estado='Pendente'");
					//oss<<PQgetvalue(res2,0,0)<<endl;		// buscar o id do jogo do 'segundo' que está pendente 
					nomes2=PQgetvalue(res2,0,0);
					cout<<nomes2<<endl;
					cout<<StringToInt(nomes)<<endl;
					if (nomes2.compare("") == 0)
						writeline(sockfd,"Esse jogador nao tem um jogo pendente");
					else if (nomes.find(nomes2) != string::npos)	// ver se encontra o id do jogo pendente na lista de id's em que ele esta desafiado. 
					{						
						cout<<"ACEITOU!"<<endl;
						writeline(sockfd,"Aceitou o desafio de "+segundo+".");
						executeSQL("UPDATE desafia SET aceita='true' WHERE id_jogo='"+nomes2+"'");
					}
					else
						writeline(sockfd,"Esse jogador não lhe fez um desafio");
					}
				
				else
					writeline(sockfd,"Executou mal o comando");
				
				}
				
			else
				writeline(sockfd,"Precisa de estar logged in para executar este comando.");
			
			comando=segundo=terceiro=nomes=nomes2=nomes3="";
			pthread_mutex_unlock(&mutex);
			}
		
		
		else if (comando.find("\\start")==0)
		{
			pthread_mutex_lock(&mutex);
			
			if(usernames.find(sockfd) != usernames.end())
			{
				PGresult* res = executeSQL("SELECT id FROM jogo WHERE username='"+usernames[sockfd]+"' AND estado='Pendente'");
				nomes=PQgetvalue(res,0,0);
				if(nomes.compare("")!=0)
				{
					PGresult* res2 = executeSQL("SELECT username FROM desafia WHERE aceita='true' AND id_jogo='"+nomes+"'");
					for (int row = 0; row < PQntuples(res2); row++)
						oss<<PQgetvalue(res2,row,0)<<"|";
					
					nomes2=oss.str();
					vector<string> utilizador = split(nomes2, '|');
					for(int p = 0; p < utilizador.size(); p++)
					{	
						executeSQL("UPDATE jogador SET estado='true' WHERE username='"+utilizador[p]+"'");
						writeline(sockets[utilizador[p]],"O jogo criado por "+usernames[sockfd]+" vai começar!");
					
					}
					
					//lança  thread
					
					pos = StringToInt(nomes);
					
					pthread_t thread2;
					pthread_create(&thread2, NULL, Jogo, &pos);
					
					executeSQL("UPDATE jogo SET estado='A jogar' WHERE id='"+nomes+"'");
					
				}
				else
					writeline(sockfd,"Precisa de ter um jogo criado para executar este comando.");
			
			}	
			else	
				writeline(sockfd, "Precisa de estar logged in para executar este comando.");
			
			comando=segundo=terceiro=nomes=nomes2=nomes3="";
			pthread_mutex_unlock(&mutex);
		}
		else if (comando.find("\\answer")==0)
        {
            pthread_mutex_lock(&mutex);
            ostringstream oss2, oss3;
            if ((segundo.compare("") == 0) || (terceiro.compare("") != 0))
                writeline(sockfd, "Executou mal o comando");
            
            else if (usernames.find(sockfd) == usernames.end())
                writeline(sockfd, "Precisa de estar logged in para executar este comando");
            
            else
            {
                PGresult* res = executeSQL("SELECT estado FROM jogador WHERE username='"+usernames[sockfd]+"'");
                    oss<<PQgetvalue(res,0,0);       
                nomes=oss.str();    
                
                if (nomes.compare("f") == 0)
                    writeline(sockfd, "Precisa de estar a jogar para executar este comando");
                else if (nomes.compare("t") == 0)
                {
                    PGresult* res2 = executeSQL("SELECT id_jogo FROM desafia WHERE username='"+usernames[sockfd]+"' AND aceita=true ORDER BY id_jogo DESC");
                    oss2<<PQgetvalue(res2,0,0);
                    nomes2 = oss2.str();
                    executeSQL("INSERT INTO resposta VALUES(default,"+nomes2+",'"+usernames[sockfd]+"','"+segundo+"')");
					PGresult* res3 = executeSQL("SELECT poscerta FROM ronda WHERE id_jogo='"+nomes2+"' ORDER BY id ASC");
					oss3<<PQgetvalue(res3,0,0);
					nomes3=oss3.str();
					if(segundo.compare(nomes3)==0)
						writeline(sockfd, "Parabens, acertou!");
					else
						writeline(sockfd, "Errou!");
                }
                        
            }
            
            
            
            
            comando=segundo=terceiro=nomes=nomes2=nomes3="";
            pthread_mutex_unlock(&mutex);
        }
		else
		{
            writeline(sockfd,"Ocorreu um erro");
            comando=segundo=terceiro=nomes=nomes2="";
		}
    }
    cout<<sockfd<<": "<<line<<endl;
    //broadcast(sockfd, line);
        /*counter=0;   
        sockets.erase(segundo);
        usernames.erase(sockfd);*/
    }
    
    
   
    
    clientes.erase(sockfd); 

    cout<<"Closing Socket"<<sockfd<<endl;

    close(sockfd);

}








int main(int argc, char *argv[])
{
    initDB();
    
    /* Estruturas de dados */
  int sockfd, newsockfd, port = 3277;
  socklen_t client_addr_length;
  struct sockaddr_in serv_addr, cli_addr;



    /* Inicializar o socket
     AF_INET - para indicar que queremos usar IP
     SOCK_STREAM - para indicar que queremos usar TCP
     sockfd - id do socket principal do servidor
     Se retornar < 0 ocorreu um erro */

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
    cout << "Error creating socket" << endl;
     exit(-1);
  }

    /* Criar a estrutura que guarda o endereço do servidor
     bzero - apaga todos os dados da estrutura (coloca a 0's)
     AF_INET - endereço IP
     INADDR_ANY - aceitar pedidos para qualquer IP */

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);


    /* Fazer bind do socket. Apenas nesta altura é que o socket fica ativo
     mas ainda não estamos a tentar receber ligações.
     Se retornar < 0 ocorreu um erro */

  int res = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (res < 0) {
    cout << "Error binding to socket" << endl;
    exit(-1);
  }

    /* Indicar que queremos escutar no socket com um backlog de 5 (podem
     ficar até 5 ligações pendentes antes de fazermos accept */
  listen(sockfd, 5);

  pthread_mutex_init(&mutex, NULL);

  while(true) {
    /* Aceitar uma nova ligação. O endereço do cliente fica guardado em 
       cli_addr - endereço do cliente
       newsockfd - id do socket que comunica com este cliente */
    client_addr_length = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_length);

    pthread_t thread;
    pthread_create(&thread, NULL, cliente, &newsockfd);

  }
    /* Fechar o socket principal, mutex e base de dados */
  pthread_mutex_destroy(&mutex);
  close(sockfd);
  closeDB();
  return 0; 
}