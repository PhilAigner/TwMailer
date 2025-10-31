#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ldap.h>
#include <iostream>

//global ldap variables
const char *ldapUri = "ldap://ldap.technikum-wien.at:389";
const int ldapVersion = LDAP_VERSION3;
int rc;
LDAP *ldapHandle = NULL;

using namespace std;

//initializes connection to ldap server ( from example code in lecture )
int ldap_connect()
{
   // If there's an existing connection, close it first
   if (ldapHandle != NULL)
   {
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      ldapHandle = NULL;
   }

   rc = ldap_initialize(&ldapHandle, ldapUri);
   if (rc != LDAP_SUCCESS)
   {
      cerr << "ldap_init failed" << endl;
      ldapHandle = NULL;
      return EXIT_FAILURE;
   }
   cout << "connected to LDAP server " << ldapUri << endl;
   cout << "connected to LDAP server " << ldapUri << endl;

   rc = ldap_set_option(
       ldapHandle,
       LDAP_OPT_PROTOCOL_VERSION,
       &ldapVersion);
   if (rc != LDAP_OPT_SUCCESS)
   {
      cerr << "ldap_set_option(PROTOCOL_VERSION): " << ldap_err2string(rc) << endl;
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      ldapHandle = NULL;
      return EXIT_FAILURE;
   }

   // Set network timeout to prevent hanging
   struct timeval timeout;
   timeout.tv_sec = 5;
   timeout.tv_usec = 0;
   ldap_set_option(ldapHandle, LDAP_OPT_NETWORK_TIMEOUT, &timeout);

   return EXIT_SUCCESS;
}

int ldap_login( const char *ldapBindUser, const char *ldapBindPassword ) {
   ////////////////////////////////////////////////////////////////////////////
   // start connection secure (initialize TLS)
   // https://linux.die.net/man/3/ldap_start_tls_s
   // int ldap_start_tls_s(LDAP *ld,
   //                      LDAPControl **serverctrls,
   //                      LDAPControl **clientctrls);
   // https://linux.die.net/man/3/ldap
   // https://docs.oracle.com/cd/E19957-01/817-6707/controls.html
   //    The LDAPv3, as documented in RFC 2251 - Lightweight Directory Access
   //    Protocol (v3) (http://www.faqs.org/rfcs/rfc2251.html), allows clients
   //    and servers to use controls as a mechanism for extending an LDAP
   //    operation. A control is a way to specify additional information as
   //    part of a request and a response. For example, a client can send a
   //    control to a server as part of a search request to indicate that the
   //    server should sort the search results before sending the results back
   //    to the client.

   
   if (ldap_connect() != EXIT_SUCCESS)
   {
      fprintf(stderr, "ldap_connect failed\n");
      return EXIT_FAILURE;
   }
   cout << "LDAP connection established for login." << endl;

   rc = ldap_start_tls_s(
       ldapHandle,
       NULL,
       NULL);
   if (rc != LDAP_SUCCESS)
   {
      fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      ldapHandle = NULL;
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // bind credentials
   // https://linux.die.net/man/3/lber-types
   // SASL (Simple Authentication and Security Layer)
   // https://linux.die.net/man/3/ldap_sasl_bind_s
   // int ldap_sasl_bind_s(
   //       LDAP *ld,
   //       const char *dn,
   //       const char *mechanism,
   //       struct berval *cred,
   //       LDAPControl *sctrls[],
   //       LDAPControl *cctrls[],
   //       struct berval **servercredp);

   // Construct full DN for the user
   // Format: uid=username,ou=people,dc=technikum-wien,dc=at
   char ldapBindDN[256];
   snprintf(ldapBindDN, sizeof(ldapBindDN), "uid=%s,ou=people,dc=technikum-wien,dc=at", ldapBindUser);
   
   std::cout << "Binding as user: " << ldapBindDN << std::endl;

   BerValue bindCredentials;
   bindCredentials.bv_val = (char *)ldapBindPassword;
   bindCredentials.bv_len = strlen(ldapBindPassword);

   BerValue *servercredp = NULL; // server's credentials
   rc = ldap_sasl_bind_s(
       ldapHandle,
       ldapBindDN,
       LDAP_SASL_SIMPLE,
       &bindCredentials,
       NULL,
       NULL,
       &servercredp);
   if (rc != LDAP_SUCCESS)
   {
      fprintf(stderr, "LDAP bind error: %s\n", ldap_err2string(rc));
      if (servercredp)
      {
         ber_bvfree(servercredp);
         servercredp = NULL;
      }
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      ldapHandle = NULL;
      return EXIT_FAILURE;
   }

   if (servercredp)
   {
      ber_bvfree(servercredp);
      servercredp = NULL;
   }


   // no more actions neccessary
   // close ldap connection

   ////////////////////////////////////////////////////////////////////////////
   // https://linux.die.net/man/3/ldap_unbind_ext_s
   // int ldap_unbind_ext_s(
   //       LDAP *ld,
   //       LDAPControl *sctrls[],
   //       LDAPControl *cctrls[]);
   ldap_unbind_ext_s(ldapHandle, NULL, NULL);
   ldapHandle = NULL;

   return EXIT_SUCCESS;
}