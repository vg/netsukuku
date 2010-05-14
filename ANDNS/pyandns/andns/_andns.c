/*
 * (c) Copyright 2007 Federico Tomassini aka efphe <effetom@gmail.com>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include <Python.h>
#include <andns.h>
#include <andns_lib.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    long            value;
    char            *name;
} _int_symbol;

typedef struct {
    char            *value;
    char            *name;
} _str_symbol;

static _int_symbol isymbols[]= {
    {ANDNS_NTK_REALM, "NTK_REALM"},
    {ANDNS_INET_REALM, "INET_REALM"},
    {AT_A, "AT_A"},
    {AT_PTR, "AT_PTR"},
    {AT_G, "AT_G"},
    {ANDNS_PROTO_TCP, "PROTO_TCP"},
    {ANDNS_PROTO_UDP, "PROTO_UDP"},
    {ANDNS_IPV4, "IPV4"},
    {ANDNS_IPV6, "IPV6"},
    {ANDNS_RCODE_NOERR, "RCODE_NOERR"},
    {ANDNS_RCODE_EINTRPRT, "RCODE_EINTRPRT"},
    {ANDNS_RCODE_ESRVFAIL, "RCODE_ESRVFAIL"},
    {ANDNS_RCODE_ENSDMN, "RCODE_ENSDMN"},
    {ANDNS_RCODE_ENIMPL, "RCODE_ENIMPL"},
    {ANDNS_RCODE_ERFSD, "RCODE_ERFSD"},
};

static _str_symbol ssymbols[]= {
    {ANDNS_SHARED_VERSION, "__andns_version__"},
};


static PyObject *AndnsError;

static PyObject *
AndnsData_Tuple(PyObject *self, andns_pkt_data *apd)
{
    PyObject *res;
    char *rdata;

    if (!(rdata= (char*)malloc(apd->rdlength+1))) 
        return NULL;

    memcpy(rdata, apd->rdata, apd->rdlength);
    *(rdata+ apd->rdlength)= 0;

    res= Py_BuildValue("(iiiis)", 
            apd->m, apd->wg, apd->prio, apd->service, rdata);

    free(rdata);
    return res;
}

static PyObject *
_tuple_to_pkt(PyObject *self, PyObject *args)
{
    char pktbuff[ANDNS_PKT_TOT_SZ];
    char *qstdata;
    int i, res=0;
    andns_pkt *packet;
    PyObject *answers;

    packet= create_andns_pkt();

    if (!PyArg_ParseTuple(args, "HBBBBHBBBBHs#O", &(packet->id), 
             &(packet->r), &(packet->qr), &(packet->z), &(packet->qtype),
             &(packet->ancount), &(packet->ipv), &(packet->nk), 
             &(packet->rcode), &(packet->p), &(packet->service), &(qstdata),
             &(packet->qstlength), &answers))
    {
        PyErr_SetString(AndnsError, "Parsing failed.");
        free_andns_pkt(packet);
        return NULL;       
    }

    // NOTE: We need to copy the buffer instead to use the one
    // allocated by PyArg_ParseTuple, to avoid the use of
    // free on invalid pointers.
    
    align_andns_question(packet, packet->qstlength);
    memcpy(packet->qstdata, qstdata, packet->qstlength);

    for (i = 0; i< packet->ancount; i++) {
        char *rdata;
        andns_pkt_data* answ_pkt = andns_add_answ(packet);
        PyObject *answer_tuple = PyList_GetItem(answers, i);
        if (!PyArg_ParseTuple(answer_tuple, "BBBHs#", &(answ_pkt->m),
                  &(answ_pkt->prio), &(answ_pkt->wg), &(answ_pkt->service),
                  &(rdata), &(answ_pkt->rdlength))) {
            PyErr_SetString(AndnsError, "Answers parsing failed.");
            free_andns_pkt(packet);
            return NULL;                                                  
        }
            
        // Read the note at 103 of this file.
        answ_pkt->rdata = malloc(answ_pkt->rdlength);
        memcpy(answ_pkt->rdata, rdata, answ_pkt->rdlength);
    }
    
    if (((res= a_p(packet, pktbuff)) == -1)) {
        PyErr_SetString(AndnsError, "Malformed Packet (packing).");
        free_andns_pkt(packet);
        return NULL;
    }

    free_andns_pkt(packet);
    return PyString_FromStringAndSize(pktbuff, ANDNS_PKT_TOT_SZ);
}

static PyObject * 
_pkt_to_tuple(PyObject *self, PyObject *args)
{ 
    char *pktbuff;
    int len, res, i;
    andns_pkt *packet;
    andns_pkt_data *apd;
    PyObject *list;
    PyObject *tuple;

    if (!PyArg_ParseTuple(args, "s#", &pktbuff, &len)) {
        PyErr_SetString(AndnsError, "Parsing failed.");
        return NULL;    
    }
    
    packet = create_andns_pkt();
    res= a_u(pktbuff, len, &packet);
    if (res<=0) { 
        PyErr_SetString(AndnsError, "Malformed packet.");
        return NULL;
    }

    tuple = PyTuple_New(13); 
        
    PyTuple_SetItem(tuple, 0,  PyInt_FromLong(packet->id));    
    PyTuple_SetItem(tuple, 1,  PyInt_FromLong(packet->r));
    PyTuple_SetItem(tuple, 2,  PyInt_FromLong(packet->qr));    
    PyTuple_SetItem(tuple, 3,  PyInt_FromLong(packet->z));
    PyTuple_SetItem(tuple, 4,  PyInt_FromLong(packet->qtype));    
    PyTuple_SetItem(tuple, 5,  PyInt_FromLong(packet->ancount));
    PyTuple_SetItem(tuple, 6,  PyInt_FromLong(packet->ipv));    
    PyTuple_SetItem(tuple, 7,  PyInt_FromLong(packet->nk));
    PyTuple_SetItem(tuple, 8,  PyInt_FromLong(packet->rcode));    
    PyTuple_SetItem(tuple, 9,  PyInt_FromLong(packet->p));
    PyTuple_SetItem(tuple, 10, PyInt_FromLong(packet->service));   
    PyTuple_SetItem(tuple, 11, PyString_FromStringAndSize(packet->qstdata,
                                                          packet->qstlength));
    
    if (packet->qtype == AT_PTR) 
    {
        // The query is of inverse type, thus the question data contains
        // an IPv4 or IPv6 address in binary format (read a_qst_p in andns_lib.c
        // for details), not a dotted string. So, we must convert it.

        struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
        int addr_len, qstlength = 0;
        
        if (packet->ipv == ANDNS_IPV4)
        {
            qstlength = 4;
            addr_len  = INET_ADDRSTRLEN;
        }
        else if (packet->ipv == ANDNS_IPV6)
        {
            qstlength = 16;
            addr_len  = INET6_ADDRSTRLEN;
        }

        void *leak = packet->qstdata;
        memcpy(&(addr->sin_addr), packet->qstdata, packet->qstlength);
        free(leak);
        
        packet->qstdata = malloc(addr_len);
        if (idp_inet_ntop(addr, packet->qstdata, addr_len) != 0)
        {
            PyErr_SetString(AndnsError, "Question data parsing failed.");
            free(addr);
            return NULL;
        }
        free(addr);

        // update qstlength with the length of the string
        packet->qstlength = strlen(packet->qstdata);
        // now convert again the string to a PyObject string
        PyTuple_SetItem(tuple, 11, PyString_FromStringAndSize(packet->qstdata,
                                                          packet->qstlength));
    }

    i = 0;
    list= PyList_New(packet->ancount);
    
    apd= packet->pkt_answ;
    while (apd) {
        res= AndnsData_Tuple(NULL, apd);
        if (!res) {
            break;
        }        
        PyList_SetItem(list, i, apd);
        i++;
        apd= apd->next;
    }
    
    PyTuple_SetItem(tuple, 12, list);
    
    free_andns_pkt(packet);
    return tuple;
}


static PyObject *
_py_ntk_query(PyObject *self, PyObject *args)
{
    const char *qst, *andns_server;
    int i=0, lerr=0, j=0, alen;
    andns_query Q;
    andns_pkt *ap;
    andns_pkt_data *apd;
    PyObject *res;
    PyObject **answers;
    PyObject *atuple;

    if (!PyArg_ParseTuple(args, "iiiiiiiiss", 
                &(Q.id), &(Q.recursion), &(Q.hashed), &(Q.type), &(Q.realm), 
                &(Q.proto), &(Q.service), &(Q.port), &qst, &andns_server))
        return NULL;

    if (strlen(andns_server)>= INET6_ADDRSTRLEN) {
        PyErr_SetString(AndnsError, "Invalid Andns Server (IP is needed).");
        return NULL;
    }
    if (strlen(qst)>= ANDNS_MAX_NTK_HNAME_LEN) {
        PyErr_SetString(AndnsError, "Question is too long.");
        return NULL;
    }

    strcpy(Q.andns_server, andns_server);
    strcpy(Q.question, qst);

    ap= ntk_query(&Q);
    if (!ap) {
        PyErr_SetString(AndnsError, Q.errors);
        return NULL;
    }

    alen= ap->ancount;
    answers= (PyObject**)malloc(sizeof(PyObject*) * alen);

    apd= ap->pkt_answ;
    while (apd) {
        res= AndnsData_Tuple(NULL, apd);
        if (!res) {
            lerr= 1;
            break;
        }
        i++;
        apd= apd->next;
        *(answers +i)= res;
    }

    if (lerr) {
        for (j=0; j<i; j++)
            Py_DECREF(*(answers +j));

        free(answers);
        free_andns_pkt(ap);
        PyErr_SetString(AndnsError, "Unparsable Answers.");
        return NULL;
    }

    atuple= PyTuple_New(alen);

    for (i=0; i< alen; i++)
        PyTuple_SetItem(atuple, i, *(answers+ i));

    free(answers);
    free_andns_pkt(ap);
    return atuple;
}

static PyMethodDef _andns_methods[]= {
    {"ntk_query", _py_ntk_query, METH_VARARGS, "Andns Function Wrapper."},
    {"pkt_to_tuple", _pkt_to_tuple, METH_VARARGS, "Convert wired packet to tuple."},
    {"tuple_to_pkt", _tuple_to_pkt, METH_VARARGS, "Convert tuples to wired packet."},    
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

void export_symbols(PyObject *m)
{
    int n_ssym, n_isym, i;

    n_isym= sizeof(isymbols)/sizeof(_int_symbol);
    n_ssym= sizeof(ssymbols)/sizeof(_str_symbol);

    for (i=0; i< n_isym; i++) 
        PyModule_AddIntConstant(m, isymbols[i].name, isymbols[i].value);
    for (i=0; i< n_ssym; i++) 
        PyModule_AddStringConstant(m, ssymbols[i].name, ssymbols[i].value);

    return;
}


PyMODINIT_FUNC 
init_andns(void)
{
    PyObject *m;

    AndnsError= PyErr_NewException("andns.AndnsError", NULL, NULL);
    Py_INCREF(AndnsError);
    m= Py_InitModule3("_andns", _andns_methods, "Andns Wrapper Module");
    PyModule_AddObject(m, "AndnsError", AndnsError);
    export_symbols(m);
}


