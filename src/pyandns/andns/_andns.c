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


