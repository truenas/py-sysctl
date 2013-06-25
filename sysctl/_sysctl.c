#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>

#include <Python.h>
#include <structmember.h>

typedef struct {
	PyObject_HEAD;
	PyObject *name;
	PyObject *value;
	PyBoolObject *writable;
	PyBoolObject *tuneable;
	unsigned int type;
	/* Type-specific fields go here. */
} Sysctl;


static int Sysctl_init(Sysctl *self, PyObject *args, PyObject *kwds) {

	PyObject *name=NULL, *value=NULL, *tmp;
	static char *kwlist[] = {"name", "value", "writable", "tuneable", "type", NULL};
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "OO|OOI", kwlist, &name, &value, &self->writable, &self->tuneable, &self->type))
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

	return 0;

}

static PyObject *Sysctl_repr(Sysctl *self) {
	static PyObject *format = NULL;
	PyObject *args, *result;
	format = PyString_FromString("<Sysctl: %s>");

	args = Py_BuildValue("O", self->name);
	result = PyString_Format(format, args);
	Py_DECREF(args);

	return result;
}

static void Sysctl_dealloc(Sysctl* self) {
	Py_XDECREF(self->name);
	Py_XDECREF(self->value);
	Py_XDECREF(self->writable);
	Py_XDECREF(self->tuneable);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *Sysctl_getvalue(Sysctl *self, void *closure) {
	Py_INCREF(self->value);
	return self->value;
}

static int Sysctl_setvalue(Sysctl *self, PyObject *value, void *closure) {
	if((PyObject *)self->writable == Py_False) {
		PyErr_SetString(PyExc_TypeError, "Sysctl is not writable");
		return -1;
	}
	if((PyObject *)self->tuneable == Py_True) {
		PyErr_SetString(PyExc_TypeError, "Sysctl is a tuneable");
		return -1;
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
	{"type", T_UINT, offsetof(Sysctl, type), READONLY, "Data type of sysctl"},
	{NULL}  /* Sentinel */
};

static PyTypeObject SysctlType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
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


static PyObject *new_sysctlobj(int *oid, int nlen) {

	char name[BUFSIZ];
	int qoid[CTL_MAXNAME+2], ctltype, rv;
	u_char *val;
	size_t j, len;
	u_int kind;
	PyObject *sysctlObj, *args, *kwargs, *value;

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
	kind = sysctl_type(oid, nlen);
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
			value = PyString_FromString((char *)val);
			break;
		case CTLTYPE_INT:
			value = PyInt_FromLong( *(int *) val);
			break;
		case CTLTYPE_UINT:
			value = PyInt_FromLong( *(u_int *) val);
			break;
		case CTLTYPE_LONG:
			value = PyLong_FromLong( *(long *) val);
			break;
		case CTLTYPE_ULONG:
			value = PyLong_FromUnsignedLong( *(u_long *) val);
			break;
		case CTLTYPE_S64:
			value = PyLong_FromLongLong( *(long long *) val);
			break;
		case CTLTYPE_U64:
			value = PyLong_FromUnsignedLongLong( *(unsigned long long *) val);
			break;
		default:
			value = PyString_FromString("NOT IMPLEMENTED");
			break;
	}

	args = Py_BuildValue("()");
	kwargs = Py_BuildValue("{s:s,s:O,s:O,s:O,s:I}",
		"name", name,
		"value", value,
		"writable", PyBool_FromLong(kind & CTLFLAG_WR),
		"tuneable", PyBool_FromLong(kind & CTLFLAG_TUN),
		"type", (unsigned int )ctltype);
	sysctlObj = PyObject_Call((PyObject *)&SysctlType, args, kwargs);

	free(val);

	return sysctlObj;

}

static PyObject* sysctl_filter(PyObject* self, PyObject* args, PyObject* kwds) {

	int name1[22], name2[22], i, j, len = 0, oid[CTL_MAXNAME];
	size_t l1=0, l2;
	static char *kwlist[] = {"mib", NULL};
	PyObject *list=NULL, *mib=NULL, *new=NULL;

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &mib))
		return NULL;

	name1[0] = 0;
	name1[1] = 2;

	list = PyList_New(0);

	if(mib) {
		len = name2oid(PyString_AsString(mib), oid);
		if(len < 0) {
			// Fix me, raise exception
			printf("mib not found!\n");
			exit(1);
		}
		u_int ctltype = sysctl_type(oid, len) & CTLTYPE;
		if(ctltype == CTLTYPE_NODE) {
			memcpy(name1 + 2, oid, len * sizeof(int));
			l1 = len + 2;
		} else {
			new = new_sysctlobj(oid, len);
			PyList_Append(list, new);
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

		new = new_sysctlobj(name2, l2);
		PyList_Append(list, new);

		memcpy(name1 + 2, name2, l2 * sizeof(int));
		l1 = l2 + 2;
	}

	return list;

}

static PyMethodDef SysctlMethods[] = {
	{"filter", (PyCFunction) sysctl_filter, METH_KEYWORDS, "Sysctl all"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
init_sysctl(void) {
	PyObject *m;


	SysctlType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&SysctlType) < 0)
		return;

	m = Py_InitModule("_sysctl", SysctlMethods);
	if (m == NULL)
		return;

	Py_INCREF(&SysctlType);
	PyModule_AddObject(m, "Sysctl", (PyObject *)&SysctlType);

}
