/****************************************************************************
 *                   ^     +----- |  / ^     ^ |     | +-\                  *
 *                  / \    |      | /  |\   /| |     | |  \                 *
 *                 /   \   +---   |<   | \ / | |     | |  |                 *
 *                /-----\  |      | \  |  v  | |     | |  /                 *
 *               /       \ |      |  \ |     | +-----+ +-/                  *
 ****************************************************************************
 * AFKMud Copyright 1997-2002 Alsherok. Contributors: Samson, Dwip, Whir,   *
 * Cyberfox, Karangi, Rathian, Cam, Raine, and Tarl.                        *
 *                                                                          *
 * Original SMAUG 1.4a written by Thoric (Derek Snider) with Altrag,        *
 * Blodkai, Haus, Narn, Scryn, Swordbearer, Tricops, Gorog, Rennard,        *
 * Grishnakh, Fireblade, and Nivek.                                         *
 *                                                                          *
 * Original MERC 2.1 code by Hatchet, Furey, and Kahn.                      *
 *                                                                          *
 * Original DikuMUD code by: Hans Staerfeldt, Katja Nyboe, Tom Madsen,      *
 * Michael Seifert, and Sebastian Hammer.                                   *
 ****************************************************************************
 *                          DNS Resolver Module                             *
 ****************************************************************************/

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include "mud.h"

DNS_DATA *first_cache;
DNS_DATA *last_cache;

void save_dns( void );

void prune_dns( void )
{
   DNS_DATA *cache, *cache_next;

   for( cache = first_cache; cache; cache = cache_next )
   {
	cache_next = cache->next;

	/* Stay in cache for 14 days */
	if( current_time - cache->time >= 1209600 || !str_cmp( cache->ip, "Unknown??" ) || !str_cmp( cache->name, "Unknown??" ) )
	{
	   STRFREE( cache->ip );
	   STRFREE( cache->name );
	   UNLINK( cache, first_cache, last_cache, next, prev );
	   DISPOSE( cache );
	}
   }
   save_dns();
   return;
}

void check_dns( void )
{
   if( current_time >= new_boot_time_t )
	prune_dns();
   return;
}

void add_dns( char *dhost, char *address )
{
   DNS_DATA *cache;

   CREATE( cache, DNS_DATA, 1 );
   cache->ip = STRALLOC( dhost );
   cache->name = STRALLOC( address );
   cache->time = current_time;
   LINK( cache, first_cache, last_cache, next, prev );

   save_dns();
   return;
}

char *in_dns_cache( char *ip )
{
   DNS_DATA *cache;
   static char dnsbuf[MAX_STRING_LENGTH];

   dnsbuf[0] = '\0';

   for( cache = first_cache; cache; cache = cache->next )
   {
	if( !str_cmp( ip, cache->ip ) )
	{
	   strcpy( dnsbuf, cache->name );
	   break;
	}
   }
   return dnsbuf;
}

#if defined(KEY)
#undef KEY
#endif

#define KEY( literal, field, value )					\
				if ( !str_cmp( word, literal ) )	\
				{					\
				      field = value;			\
				      fMatch = TRUE;			\
				      break;				\
				}

void fread_dns( DNS_DATA *cache, FILE *fp )
{
   char *word;
   bool fMatch;

   for ( ; ; )
   {
	word   = feof( fp ) ? "End" : fread_word( fp );
	fMatch = FALSE;

	switch ( UPPER(word[0]) )
	{
	   case '*':
	      fMatch = TRUE;
	      fread_to_eol( fp );
	   break;

	   case 'E':
	      if ( !str_cmp( word, "End" ) )
	      {
		   if( !cache->ip )
		      cache->ip = STRALLOC( "Unknown??" );
		   if( !cache->name )
			cache->name = STRALLOC( "Unknown??" );
		   return;
	      }
	   break;

	   case 'I':
	      KEY( "IP", cache->ip, fread_string( fp ) );
	   break;

	   case 'N':
	      KEY( "Name", cache->name, fread_string( fp ) );
	   break;

	   case 'T':
		KEY( "Time", cache->time, fread_number( fp ) );
	   break;
	}

	if( !fMatch )
	   bug( "fread_dns: no match: %s", word );
   }
}

void load_dns( void )
{
   char filename[256];
   DNS_DATA *cache;
   FILE *fp;
     
   first_cache = NULL;
   last_cache = NULL;

   sprintf( filename, "%s", DNS_FILE );

   if( ( fp = fopen( filename, "r" ) ) != NULL )
   {
	for ( ; ; )
	{
	   char letter;
	   char *word;

	   letter = fread_letter( fp );
	   if ( letter == '*' )
	   {
		fread_to_eol( fp );
		continue;
	   }

	   if ( letter != '#' )
	   {
		bug( "load_dns: # not found.", 0 );
		break;
	   }

	   word = fread_word( fp );
	   if ( !str_cmp( word, "CACHE" ) )
	   {
		CREATE( cache, DNS_DATA, 1 );
		fread_dns( cache, fp );
		LINK( cache, first_cache, last_cache, next, prev );
		continue;
	   }
	   else
            if ( !str_cmp( word, "END"	) )
	         break;
	   else
	   {
		bug( "load_dns: bad section: %s.", word );
		continue;
	   }
	}
        fclose( fp );
   }
   prune_dns(); /* Clean out entries beyond 14 days */
   return;
}

void save_dns( void )
{
   DNS_DATA *cache;
   FILE *fp;
   char filename[256];

   sprintf( filename, "%s", DNS_FILE );

   if ( ( fp = fopen( filename, "w" ) ) == NULL )
   {
	bug( "save_dns: fopen", 0 );
	perror( filename );
   }
   else
   {
	for( cache = first_cache; cache; cache = cache->next )
      {
	   fprintf( fp, "#CACHE\n" );
	   fprintf( fp, "IP		%s~\n",	cache->ip );
	   fprintf( fp, "Name		%s~\n",	cache->name );
	   fprintf( fp, "Time		%ld\n",	cache->time );
	   fprintf( fp, "End\n\n" );
	}
	fprintf( fp, "#END\n" );
        fclose(fp);
   }
   return;
}

/* DNS Resolver code by Trax of Forever's End */
/*
 * Almost the same as read_from_buffer...
 */
bool read_from_dns( int fd, char *buffer )
{
   static char inbuf[MAX_STRING_LENGTH*2];
   int iStart, i, j, k;

   /* Check for overflow. */
   iStart = strlen( inbuf );
   if ( iStart >= sizeof( inbuf ) - 10 )
   {
	bug( "DNS input overflow!!!", 0 );
	return FALSE;
   }

   /* Snarf input. */
   for ( ; ; )
   {
	int nRead;

	nRead = read( fd, inbuf + iStart, sizeof( inbuf ) - 10 - iStart );
	if ( nRead > 0 )
	{
	   iStart += nRead;
	   if ( inbuf[iStart-2] == '\n' || inbuf[iStart-2] == '\r' )
		break;
	}
	else if ( nRead == 0 )
	{
	   return FALSE;
	}
	else if ( errno == EWOULDBLOCK )
	   break;
	else
	{
	   perror( "Read_from_dns" );
	   return FALSE;
	}
   }

   inbuf[iStart] = '\0';

   /*
    * Look for at least one new line.
    */
   for( i = 0; inbuf[i] != '\n' && inbuf[i] != '\r'; i++ )
   {
	if( inbuf[i] == '\0' )
	   return FALSE;
   }

   /*
    * Canonical input processing.
    */
   for( i = 0, k = 0; inbuf[i] != '\n' && inbuf[i] != '\r'; i++ )
   {
	if ( inbuf[i] == '\b' && k > 0 )
	   --k;
	else if( isascii( inbuf[i] ) && isprint( inbuf[i] ) )
	   buffer[k++] = inbuf[i];
   }

   /*
    * Finish off the line.
    */
   if( k == 0 )
	buffer[k++] = ' ';
   buffer[k] = '\0';

   /*
    * Shift the input buffer.
    */
   while( inbuf[i] == '\n' || inbuf[i] == '\r' )
	i++;
   for ( j = 0; ( inbuf[j] = inbuf[i+j] ) != '\0'; j++ )
	;
    
   return TRUE;
}

/* DNS Resolver code by Trax of Forever's End */
/*
 * Process input that we got from resolve_dns.
 */
void process_dns( DESCRIPTOR_DATA *d )
{
   char address[MAX_INPUT_LENGTH];
   int status;
    
   address[0] = '\0';
    
   if( !read_from_dns( d->ifd, address ) || address[0] == '\0' )
    	return;
    
   if ( address[0] != '\0' )
   {
	add_dns( d->host, address ); /* Add entry to DNS cache */
	STRFREE( d->host );
      d->host = STRALLOC( address );
        /*
        if( d->character )
	{
	   STRFREE( d->character->pcdata->lasthost );
	   d->character->pcdata->lasthost = STRALLOC( address );
	}
        */
   }
    
   /* close descriptor and kill dns process */
   if( d->ifd != -1 )
   {
      close( d->ifd );
      d->ifd = -1;
   }

   /* 
    * we don't have to check here, 
    * cos the child is probably dead already. (but out of safety we do)
    * 
    * (later) I found this not to be true. The call to waitpid( ) is
    * necessary, because otherwise the child processes become zombie
    * and keep lingering around... The waitpid( ) removes them.
    */
   if( d->ipid != -1 )
   {
      waitpid( d->ipid, &status, 0 );
      d->ipid = -1;
   }
   return;
}

/* DNS Resolver hook. Code written by Trax of Forever's End */
void resolve_dns( DESCRIPTOR_DATA *d, long ip )
{
   int fds[2];
   pid_t pid;
    
   /* create pipe first */
   if( pipe( fds )!=0 )
   {
      perror( "resolve_dns: pipe: " );
      return;
   }
    
   if( dup2( fds[1], STDOUT_FILENO ) != STDOUT_FILENO )
   {
      perror( "resolve_dns: dup2(stdout): " );
      return;
   }
    
   if( ( pid = fork() ) > 0 )
   {
    	/* parent process */
    	d->ifd  = fds[0];
    	d->ipid = pid;
      close( fds[1] );
   }
   else if( pid == 0 )
   {
    	/* child process */
	char str_ip[64];
	int i;
        
    	d->ifd  = fds[0]; 
    	d->ipid = pid;

      for( i = 2; i < 255; ++i ) close(i);

	sprintf( str_ip, "%ld", ip );
    	execl( "../src/resolver", "AFKMud Resolver", str_ip, NULL );
    	/* Still here --> hmm. An error. */
    	bug( "resolve_dns: Exec failed; Closing child.", 0 );
    	d->ifd  = -1;
    	d->ipid = -1;    	
    	exit( 0 );
   }
   else 
   {
    	/* error */
    	perror( "resolve_dns: failed fork" );
    	close( fds[0] );
    	close( fds[1] );
   }
}

void do_cache( CHAR_DATA *ch, char *argument )
{
   DNS_DATA *cache;
   int ip = 0;

   send_to_pager( "&YCached DNS Information\n\r", ch );
   send_to_pager( "IP               | Address\n\r", ch );
   send_to_pager( "------------------------------------------------------------------------------\n\r", ch );
   for( cache = first_cache; cache; cache = cache->next )
   {
	pager_printf( ch, "&W%16.16s  &Y%s\n\r", cache->ip, cache->name );
	ip++;
   }
   pager_printf( ch, "\n\r&W%d IPs in the cache.\n\r", ip );
   return;
}
