#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>

#include <Python.h>
#include <structmember.h>

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

static PyObject *
error_out(PyObject *m) {
	struct module_state *st = GETSTATE(m);
	PyErr_SetString(st->error, "something bad happened");
	return NULL;
}

typedef struct {
	PyObject_HEAD;
	PyObject *name;
	PyObject *value;
	PyObject *writable;
	PyObject *tuneable;
	PyObject *oid;
	unsigned int type;
} Sysctl;


static int Sysctl_init(Sysctl *self, PyObject *args, PyObject *kwds) {

	PyObject *name=NULL, *value=NULL, *writable=NULL, *tuneable=NULL, *oid=NULL, *tmp;
	static char *kwlist[] = {"name", "value", "writable", "tuneable", "type", "oid", NULL};
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "OO|OOIO", kwlist, &name, &value, &writable, &tuneable, &self->type, &oid))
		return -1;

	if(name) {
		tmp = self->name;
		Py_INCREF(name);
		self->name = name;
		Py_XDECREF(tmp);
	}
	if(value) {
		tmp = self->value;
		Py_INCREF(value);
		self->value = value;
		Py_XDECREF(tmp);
	}

	if(writable) {
		tmp = self->writable;
		Py_INCREF(writable);
		self->writable = writable;
		Py_XDECREF(tmp);
	}

	if(tuneable) {
		tmp = self->tuneable;
		Py_INCREF(tuneable);
		self->tuneable = tuneable;
		Py_XDECREF(tmp);
	}

	if(oid) {
		tmp = self->oid;
		Py_INCREF(oid);
		self->oid = oid;
		Py_XDECREF(tmp);
	}

	return 0;

}

static PyObject *Sysctl_repr(Sysctl *self) {
	static PyObject *format = NULL;
	PyObject *args, *result;
	format = PyUnicode_FromString("<Sysctl: %s>");

	args = Py_BuildValue("O", self->name);
	result = PyUnicode_Format(format, args);
	Py_DECREF(args);
	Py_DECREF(format);

	return result;
}

static void Sysctl_dealloc(Sysctl* self) {
	Py_XDECREF(self->name);
	Py_XDECREF(self->value);
	Py_XDECREF(self->writable);
	Py_XDECREF(self->tuneable);
	Py_XDECREF(self->oid);
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *Sysctl_getvalue(Sysctl *self, void *closure) {
	Py_INCREF(self->value);
	return self->value;
}

static int Sysctl_setvalue(Sysctl *self, PyObject *value, void *closure) {
	void *newval = NULL;
	size_t newsize = 0;

	if((PyObject *)self->writable == Py_False) {
		PyErr_SetString(PyExc_TypeError, "Sysctl is not writable");
		return -1;
	}
	if((PyObject *)self->tuneable == Py_True) {
		PyErr_SetString(PyExc_TypeError, "Sysctl is a tuneable");
		return -1;
	}
	switch(self->type) {
		case CTLTYPE_INT:
		case CTLTYPE_UINT:
			if(value->ob_type != &PyLong_Type) {
				PyErr_SetString(PyExc_TypeError, "Invalid type");
				return -1;
			}
			newval = malloc(sizeof(int));
			newsize = sizeof(int);
			*((int *) newval) = PyLong_AsLong(value);
			break;
		case CTLTYPE_LONG:
		case CTLTYPE_ULONG:
			if(value->ob_type != &PyLong_Type) {
				PyErr_SetString(PyExc_TypeError, "Invalid type");
				return -1;
			}
			newval = malloc(sizeof(long));
			newsize = sizeof(long);
			*((long *) newval) = PyLong_AsLong(value);
			break;
#ifdef CTLTYPE_S64
		case CTLTYPE_S64:
		case CTLTYPE_U64:
#else
		case CTLTYPE_QUAD:
#endif
			if(value->ob_type != &PyLong_Type) {
				PyErr_SetString(PyExc_TypeError, "Invalid type");
				return -1;
			}
			newval = malloc(sizeof(long long));
			newsize = sizeof(long long);
			*((long long *) newval) = PyLong_AsLongLong(value);
			break;
		default:
			break;
	}

	if(newval) {
		int *oid, i=0;
		ssize_t size;
		size = PyList_Size((PyObject *) self->oid);
		oid = calloc(sizeof(int), size);
		for(i=0;i<size;i++) {
			oid[i] = (u_int) PyLong_AsLong(PyList_GetItem(self->oid, i));
		}
		if(sysctl(oid, size, 0, 0, newval, newsize) == -1) {

			PyErr_SetString(PyExc_TypeError, "Failed to set sysctl");
			free(newval);
			free(oid);
			return -1;
		}
		free(newval);
		free(oid);
	}

	Py_DECREF(self->value);
	Py_INCREF(value);
	self->value = value;
	return 0;
}

static PyGetSetDef Sysctl_getseters[] = {
	{"value", (getter)Sysctl_getvalue, (setter)Sysctl_setvalue, "sysctl value", NULL},
	{NULL}  /* Sentinel */
};

static PyMemberDef Sysctl_members[] = {
	{"name", T_OBJECT_EX, offsetof(Sysctl, name), READONLY, "name"},
	{"writable", T_OBJECT_EX, offsetof(Sysctl, writable), READONLY, "Can be written"},
	{"tuneable", T_OBJECT_EX, offsetof(Sysctl, tuneable), READONLY, "Tuneable"},
	{"oid", T_OBJECT_EX, offsetof(Sysctl, oid), READONLY, "OID MIB"},
	{"type", T_UINT, offsetof(Sysctl, type), READONLY, "Data type of sysctl"},
	{NULL}  /* Sentinel */
};

static PyTypeObject SysctlType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Sysctl",                  /*tp_name*/
	sizeof(Sysctl),            /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)
	Sysctl_dealloc,            /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	(PyObject *(*)(PyObject *))
	Sysctl_repr,               /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"Sysctl objects",           /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	0,                         /* tp_methods */
	Sysctl_members,            /* tp_members */
	Sysctl_getseters,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)Sysctl_init,     /* tp_init */
	0,                         /* tp_alloc */
	//Sysctl_new,                 /* tp_new */
};



static int
name2oid(char *name, int *oidp)
{
	int oid[2];
	int i;
	size_t j;

	oid[0] = 0;
	oid[1] = 3;

	j = CTL_MAXNAME * sizeof(int);
	i = sysctl(oid, 2, oidp, &j, name, strlen(name));
	if (i < 0)
		return (i);
	j /= sizeof(int);
	return (j);
}


static u_int sysctl_type(int *oid, int len) {

	int qoid[CTL_MAXNAME+2], i;
	u_char buf[BUFSIZ];
	size_t j;

	qoid[0] = 0;
	qoid[1] = 4;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof(buf);
	i = sysctl(qoid, len + 2, buf, &j, 0, 0);
	if(i < 0) {
		printf("fatal error sysctl_type\n");
		exit(-1);
	}

	return *(u_int *) buf;
}


static PyObject *new_sysctlobj(int *oid, int nlen, u_int kind) {

	char name[BUFSIZ];
	int qoid[CTL_MAXNAME+2], ctltype, rv, i;
	u_char *val;
	size_t j, len;
	PyObject *sysctlObj, *args, *kwargs, *value, *oidobj, *oidentry, *writable, *tuneable;

	bzero(name, BUFSIZ);
	qoid[0] = 0;
	qoid[1] = 1;
	memcpy(qoid + 2, oid, nlen * sizeof(int));

	j = sizeof(name);
	rv = sysctl(qoid, nlen + 2, name, &j, 0, 0);
	if(rv == -1) {
		printf("error");
		exit(1);
	}
	ctltype = kind & CTLTYPE;
	j = 0;
	sysctl(oid, nlen, 0, &j, 0, 0);
	j += j; /* double size just to be sure */

	val = malloc(j + 1);
	len = j;

	sysctl(oid, nlen, val, &len, 0, 0);

	switch(ctltype) {
		case CTLTYPE_STRING:
			val[len] = '\0';
			value = PyUnicode_FromString((char *)val);
			break;
		case CTLTYPE_INT:
			value = PyLong_FromLong( *(int *) val);
			break;
		case CTLTYPE_UINT:
			value = PyLong_FromLong( *(u_int *) val);
			break;
		case CTLTYPE_LONG:
			value = PyLong_FromLong( *(long *) val);
			break;
		case CTLTYPE_ULONG:
			value = PyLong_FromUnsignedLong( *(u_long *) val);
			break;
#ifdef CTLTYPE_S64
		case CTLTYPE_S64:
			value = PyLong_FromLongLong( *(long long *) val);
			break;
		case CTLTYPE_U64:
			value = PyLong_FromUnsignedLongLong( *(unsigned long long *) val);
			break;
#else
		case CTLTYPE_QUAD:
			value = PyLong_FromLongLong( *(long long *) val);
			break;
#endif
		default:
			value = PyUnicode_FromString("NOT IMPLEMENTED");
			break;
	}

	oidobj = PyList_New(0);
	for(i=0;i<nlen;i++) {
		oidentry = PyLong_FromLong(oid[i]);
		PyList_Append(oidobj, oidentry);
		Py_DECREF(oidentry);
	}
	writable = PyBool_FromLong(kind & CTLFLAG_WR);
	tuneable = PyBool_FromLong(kind & CTLFLAG_TUN);
	args = Py_BuildValue("()");
	kwargs = Py_BuildValue("{s:s,s:O,s:O,s:O,s:I,s:O}",
		"name", name,
		"value", value,
		"writable", writable,
		"tuneable", tuneable,
		"type", (unsigned int )ctltype,
		"oid", oidobj);
	sysctlObj = PyObject_Call((PyObject *)&SysctlType, args, kwargs);
	Py_DECREF(args);
	Py_DECREF(kwargs);
	Py_DECREF(oidobj);
	Py_DECREF(value);
	Py_DECREF(writable);
	Py_DECREF(tuneable);

	free(val);

	return sysctlObj;

}

static PyObject* sysctl_filter(PyObject* self, PyObject* args, PyObject* kwds) {

	int name1[22], name2[22], i, j, len = 0, oid[CTL_MAXNAME];
	size_t l1=0, l2;
	static char *kwlist[] = {"mib", "writable", NULL};
	char *mib;
	PyObject *list=NULL, *writable=NULL, *new=NULL;
	u_int kind, ctltype;

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "|zO", kwlist, &mib, &writable))
		return NULL;

	name1[0] = 0;
	name1[1] = 2;

	list = PyList_New(0);
	if(mib != NULL && mib[0] != '\n' && strlen(mib) > 0) {
		len = name2oid(mib, oid);
		if(len < 0) {
			//PyErr_SetString(PyExc_TypeError, "mib not found");
			//return -1;
		} else {
			kind = sysctl_type(oid, len);
			ctltype = kind & CTLTYPE;
			if(ctltype == CTLTYPE_NODE) {
				memcpy(name1 + 2, oid, len * sizeof(int));
				l1 = len + 2;
			} else {
				new = new_sysctlobj(oid, len, kind);
				PyList_Append(list, new);
			}
		}
	} else {
		name1[2] = 1;
		l1 = 3;
	}

	for (;;) {
		l2 = sizeof(name2);
		j = sysctl(name1, l1, name2, &l2, 0, 0);
		if (j < 0) {
			if (errno == ENOENT)
				return list;
			//else
			//	err(1, "sysctl(getnext) %d %zu", j, l2);
		}

		l2 /= sizeof(int);

		if (len < 0 || l2 < (unsigned int)len)
			return list;

		for (i = 0; i < len ; i++)
			if (name2[i] != oid[i])
				return list;

		kind = sysctl_type(name2, l2);
		ctltype = kind & CTLTYPE;
		if( (PyObject *)writable == Py_True && (kind & CTLFLAG_WR) == 0 ) {
			memcpy(name1 + 2, name2, l2 * sizeof(int));
			l1 = l2 + 2;
			continue;
		} else if( (PyObject *)writable == Py_False && (kind & CTLFLAG_WR) > 0 ) {
			memcpy(name1 + 2, name2, l2 * sizeof(int));
			l1 = l2 + 2;
			continue;
		}

		new = new_sysctlobj(name2, l2, kind);
		PyList_Append(list, new);
		Py_DECREF(new);

		memcpy(name1 + 2, name2, l2 * sizeof(int));
		l1 = l2 + 2;
	}
	free(mib);
	Py_DECREF(writable);

	return list;

}

static PyMethodDef SysctlMethods[] = {
	{"filter", (PyCFunction) sysctl_filter, METH_VARARGS|METH_KEYWORDS, "Sysctl all"},
	{"error_out", (PyCFunction) error_out, METH_NOARGS, NULL},
	{NULL, NULL}        /* Sentinel */
};


#if PY_MAJOR_VERSION >= 3

static int sysctl_traverse(PyObject *m, visitproc visit, void *arg) {
	Py_VISIT(GETSTATE(m)->error);
	return 0;
}

static int sysctl_clear(PyObject *m) {
	Py_CLEAR(GETSTATE(m)->error);
	return 0;
}


static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"_sysctl",
	NULL,
	sizeof(struct module_state),
	SysctlMethods,
	NULL,
	sysctl_traverse,
	sysctl_clear,
	NULL
};

#define INITERROR return NULL

PyObject *
PyInit__sysctl(void)

#else
#define INITERROR return

PyMODINIT_FUNC
init_sysctl(void)
#endif
{
	PyObject *m;
	SysctlType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&SysctlType) < 0)
		INITERROR;

#if PY_MAJOR_VERSION >= 3
	m = PyModule_Create(&moduledef);
#else
	m = Py_InitModule("_sysctl", SysctlMethods);
#endif
	if (m == NULL)
		INITERROR;

	struct module_state *st = GETSTATE(m);
	st->error = PyErr_NewException("_sysctl.Error", NULL, NULL);
	if (st->error == NULL) {
		Py_DECREF(m);
		INITERROR;
	}

	Py_INCREF(&SysctlType);
	PyModule_AddObject(m, "Sysctl", (PyObject *)&SysctlType);

	PyModule_AddIntConstant(m, "CTLTYPE", CTLTYPE);
	PyModule_AddIntConstant(m, "CTLTYPE_NODE", CTLTYPE_NODE);
	PyModule_AddIntConstant(m, "CTLTYPE_INT", CTLTYPE_INT);
	PyModule_AddIntConstant(m, "CTLTYPE_STRING", CTLTYPE_STRING);
	PyModule_AddIntConstant(m, "CTLTYPE_OPAQUE", CTLTYPE_OPAQUE);
	PyModule_AddIntConstant(m, "CTLTYPE_STRUCT", CTLTYPE_STRUCT);
	PyModule_AddIntConstant(m, "CTLTYPE_UINT", CTLTYPE_UINT);
	PyModule_AddIntConstant(m, "CTLTYPE_LONG", CTLTYPE_LONG);
	PyModule_AddIntConstant(m, "CTLTYPE_ULONG", CTLTYPE_ULONG);
#ifdef CTLTYPE_S64
	PyModule_AddIntConstant(m, "CTLTYPE_S64", CTLTYPE_S64);
	PyModule_AddIntConstant(m, "CTLTYPE_U64", CTLTYPE_U64);
#else
	PyModule_AddIntConstant(m, "CTLTYPE_QUAD", CTLTYPE_QUAD);
#endif

#if PY_MAJOR_VERSION >= 3
	return m;
#endif

}
