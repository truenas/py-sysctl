#include <sys/param.h>
#include <sys/sysctl.h>

#define	PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

/*
 * CTL_SYSCTL first defined in FreeBSD 12.2
 */
#ifndef CTL_SYSCTL
#define	CTL_SYSCTL		0	/* "magic" numbers */
#define	CTL_SYSCTL_NAME		1	/* string name of OID */
#define	CTL_SYSCTL_NEXT		2	/* next OID */
#define	CTL_SYSCTL_NAME2OID	3	/* int array of name */
#define	CTL_SYSCTL_OIDFMT	4	/* OID's kind and format */
#define	CTL_SYSCTL_OIDDESCR	5	/* OID's description */
#endif

/*
 * Py_UNREACHABLE first defined in Python 3.7
 */
#ifndef Py_UNREACHABLE
#define	Py_UNREACHABLE()	abort()
#endif

struct module_state {
	PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define PyInt_Type PyLong_Type
#define PyInt_FromLong PyLong_FromLong
#define GETSTATE(m) ((struct module_state *)PyModule_GetState(m))
PyMODINIT_FUNC PyInit__sysctl(void);
#else
#define GETSTATE(m) ((void)m, &_state)
static struct module_state _state;
PyMODINIT_FUNC init_sysctl(void);
#endif

typedef struct {
	PyObject_HEAD
	PyObject *name;
	PyObject *value;
	PyObject *description;
	PyObject *writable;
	PyObject *tuneable;
	PyObject *oid;
	struct {
		int *oid;
		char *fmt;
		u_int len;
		u_int type;
	} private;
} Sysctl;

static PyObject *
error_out(PyObject *m)
{
	struct module_state *st = GETSTATE(m);

	PyErr_SetString(st->error, "something bad happened");

	return (NULL);
}

static int
Sysctl_init(Sysctl *self, PyObject *args, PyObject *kwds)
{
	PyObject *tmp,
	    *name = NULL,
	    *value = NULL,
	    *description = NULL,
	    *writable = NULL,
	    *tuneable = NULL,
	    *oid = NULL;
	static char *kwlist[] = {
		"name",
		"value",
		"description",
		"writable",
		"tuneable",
		"oid",
		"type",
		NULL
	};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OOOOOI", kwlist,
	    &name, &value, &description, &writable, &tuneable, &oid,
	    &self->private.type))
		return (-1);

	if (name) {
		tmp = self->name;
		Py_INCREF(name);
		self->name = name;
		Py_XDECREF(tmp);
	}
	if (value) {
		tmp = self->value;
		Py_INCREF(value);
		self->value = value;
		Py_XDECREF(tmp);
	}
	if (description) {
		tmp = self->description;
		Py_INCREF(description);
		self->description = tmp;
		Py_XDECREF(tmp);
	}
	if (writable) {
		tmp = self->writable;
		Py_INCREF(writable);
		self->writable = writable;
		Py_XDECREF(tmp);
	}
	if (tuneable) {
		tmp = self->tuneable;
		Py_INCREF(tuneable);
		self->tuneable = tuneable;
		Py_XDECREF(tmp);
	}
	if (oid) {
		tmp = self->oid;
		Py_INCREF(oid);
		self->oid = oid;
		Py_XDECREF(tmp);
	}

	return (0);
}

static PyObject *
Sysctl_repr(Sysctl *self)
{
	static PyObject *format = NULL;
	PyObject *args, *result;
	format = PyUnicode_FromString("<Sysctl: %s>");

	args = Py_BuildValue("O", self->name);
	result = PyUnicode_Format(format, args);
	Py_DECREF(args);
	Py_DECREF(format);

	return (result);
}

static void
Sysctl_dealloc(Sysctl *self)
{
	free(self->private.oid);
	free(self->private.fmt);
	Py_XDECREF(self->name);
	Py_XDECREF(self->value);
	Py_XDECREF(self->writable);
	Py_XDECREF(self->tuneable);
	Py_XDECREF(self->oid);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
Sysctl_getvalue(Sysctl *self, void *closure __unused)
{
	static const size_t ctl_size[CTLTYPE + 1] = {
		[CTLTYPE_INT] = sizeof(int),
		[CTLTYPE_UINT] = sizeof(u_int),
		[CTLTYPE_LONG] = sizeof(long),
		[CTLTYPE_ULONG] = sizeof(u_long),
#ifdef CTLTYPE_S64
		[CTLTYPE_S64] = sizeof(int64_t),
		[CTLTYPE_U64] = sizeof(int64_t),
#else
		[CTLTYPE_QUAD] = sizeof(int64_t),
#endif
	};
	PyThreadState *save;
	PyObject *value, *entry;
	u_char *val, *p;
	const int *oid;
	size_t len, intlen;
	u_int nlen;

	if (self->value != NULL) {
		Py_INCREF(self->value);
		return (self->value);
	}

	intlen = ctl_size[self->private.type];
	oid = self->private.oid;
	nlen = self->private.len;
	save = PyEval_SaveThread();
	val = NULL;
	len = 0;
	while (sysctl(oid, nlen, val, &len, NULL, 0) != 0 || val == NULL) {
		if (errno == EISDIR) {
			free(val);
			PyEval_RestoreThread(save);
			self->value = Py_None;
			Py_INCREF(self->value);
			Py_INCREF(self->value);
			return (self->value);
		}
		if (val != NULL && errno != 0 && errno != ENOMEM) {
			free(val);
			PyEval_RestoreThread(save);
			return (PyErr_SetFromErrno(PyExc_OSError));
		}
		p = realloc(val, len + 1);
		if (p == NULL) {
			free(val);
			PyEval_RestoreThread(save);
			return (PyErr_SetFromErrno(PyExc_OSError));
		}
		val = p;
	}
	PyEval_RestoreThread(save);
	val[len] = '\0';

	switch (self->private.type) {
	case CTLTYPE_STRING:
		value = PyUnicode_FromString((const char *)val);
		break;
	case CTLTYPE_INT:
		if (len > intlen) {
			value = PyList_New(0);
			while (len >= intlen) {
				const int *valp = (const void *)p;
				entry = PyLong_FromLong(*valp);
				PyList_Append(value, entry);
				Py_DECREF(entry);
				len -= intlen;
				p += intlen;
			}
		} else {
			const int *valp = (const void *)val;
			value = PyLong_FromLong(*valp);
		}
		break;
	case CTLTYPE_UINT:
		if (len > intlen) {
			value = PyList_New(0);
			while (len >= intlen) {
				const unsigned int *valp = (const void *)p;
				entry = PyLong_FromLong(*valp);
				PyList_Append(value, entry);
				Py_DECREF(entry);
				len -= intlen;
				p += intlen;
			}
		} else {
			const unsigned int *valp = (const void *)val;
			value = PyLong_FromLong(*valp);
		}
		break;
	case CTLTYPE_LONG:
		if (len > intlen) {
			value = PyList_New(0);
			while (len >= intlen) {
				const long *valp = (const void *)p;
				entry = PyLong_FromLong(*valp);
				PyList_Append(value, entry);
				Py_DECREF(entry);
				len -= intlen;
				p += intlen;
			}
		} else {
			const long *valp = (const void *)val;
			value = PyLong_FromLong(*valp);
		}
		break;
	case CTLTYPE_ULONG:
		if (len > intlen) {
			value = PyList_New(0);
			while (len >= intlen) {
				const unsigned long *valp = (const void *)p;
				entry = PyLong_FromUnsignedLong(*valp);
				PyList_Append(value, entry);
				Py_DECREF(entry);
				len -= intlen;
				p += intlen;
			}
		} else {
			const unsigned long *valp = (const void *)val;
			value = PyLong_FromUnsignedLong(*valp);
		}
		break;
#ifdef CTLTYPE_S64
	case CTLTYPE_S64:
		if (len > intlen) {
			value = PyList_New(0);
			while (len >= intlen) {
				const long long *valp = (const void *)p;
				entry = PyLong_FromLongLong(*valp);
				PyList_Append(value, entry);
				Py_DECREF(entry);
				len -= intlen;
				p += intlen;
			}
		} else {
			const long long *valp = (const void *)val;
			value = PyLong_FromLongLong(*valp);
		}
		break;
	case CTLTYPE_U64:
		if (len > intlen) {
			value = PyList_New(0);
			while (len >= intlen) {
				const unsigned long long *valp =
				    (const void *)p;
				entry = PyLong_FromUnsignedLongLong(*valp);
				PyList_Append(value, entry);
				Py_DECREF(entry);
				len -= intlen;
				p += intlen;
			}
		} else {
			const unsigned long long *valp = (const void *)val;
			value = PyLong_FromUnsignedLongLong(*valp);
		}
		break;
#else
	case CTLTYPE_QUAD:
		value = PyLong_FromLongLong(*(const long long *)val);
		break;
#endif
	case CTLTYPE_OPAQUE:
		if (strcmp(self->private.fmt, "S,clockinfo") == 0) {
			const struct clockinfo *ci = (const void *)val;

			value = PyDict_New();

			entry = PyInt_FromLong(ci->hz);
			PyDict_SetItemString(value, "hz", entry);
			Py_DECREF(entry);

			entry = PyInt_FromLong(ci->tick);
			PyDict_SetItemString(value, "tick", entry);
			Py_DECREF(entry);

			entry = PyInt_FromLong(ci->profhz);
			PyDict_SetItemString(value, "profhz", entry);
			Py_DECREF(entry);

			entry = PyInt_FromLong(ci->stathz);
			PyDict_SetItemString(value, "stathz", entry);
			Py_DECREF(entry);

			break;
		}
		/* fallthrough */
	default:
		value = PyByteArray_FromStringAndSize((const char *)val,
		    (Py_ssize_t)len);
		break;
	}
	free(val);

	Py_INCREF(value);
	self->value = value;
	return (value);
}

static char *
convert_pyobject_str_to_char(PyObject *obj)
{
	char *bytes = NULL;
	PyObject *str = NULL;

	if (PyUnicode_CheckExact(obj)) {
		str = PyUnicode_AsEncodedString(obj, "utf-8", "~E~");
		if (str != NULL) {
			bytes = PyBytes_AS_STRING(str);
			Py_DECREF(str);
		}
	}

	return (bytes);
}

static int
Sysctl_setvalue(Sysctl *self, PyObject *value, void *closure __unused)
{
	void *newval = NULL;
	size_t newsize = 0;
	char *newvalstr = NULL;

	if ((PyObject *)self->writable == Py_False) {
		PyErr_SetString(PyExc_TypeError, "Sysctl is not writable");
		return (-1);
	}
	switch (self->private.type) {
	case CTLTYPE_INT:
	case CTLTYPE_UINT:
		if (value->ob_type != &PyInt_Type) {
			PyErr_SetString(PyExc_TypeError, "Invalid type");
			return (-1);
		}
		newsize = sizeof(int);
		newval = malloc(newsize);
		*((int *)newval) = (int)PyLong_AsLong(value);
		break;
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
		if (value->ob_type != &PyLong_Type &&
		    value->ob_type != &PyInt_Type) {
			PyErr_SetString(PyExc_TypeError, "Invalid type");
			return (-1);
		}
		newsize = sizeof(long);
		newval = malloc(newsize);
		*((long *)newval) = PyLong_AsLong(value);
		break;
#ifdef CTLTYPE_S64
	case CTLTYPE_S64:
	case CTLTYPE_U64:
#else
	case CTLTYPE_QUAD:
#endif
		if (value->ob_type != &PyLong_Type) {
			PyErr_SetString(PyExc_TypeError, "Invalid type");
			return (-1);
		}
		newsize = sizeof(long long);
		newval = malloc(newsize);
		*((long long *)newval) = PyLong_AsLongLong(value);
		break;
	case CTLTYPE_STRING:
		newvalstr = convert_pyobject_str_to_char(value);
		if (newvalstr) {
			newsize = strlen(newvalstr);
			newval = newvalstr;
		} else {
			PyErr_SetString(PyExc_TypeError, "Invalid type");
			return (-1);
		}
		break;
	case CTLTYPE_OPAQUE:
		if (!PyByteArray_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "Invalid type");
			return (-1);
		}
		newsize = PyByteArray_Size(value);
		newval = malloc(newsize);
		memcpy(newval, PyByteArray_AsString(value), newsize);
		break;
	default:
		break;
	}

	if (newval) {
		int rv, *oid;
		ssize_t size;

		size = PyList_Size(self->oid);
		assert(size >= 0);
		oid = calloc(sizeof(int), (size_t)size);
		for (int i = 0; i < size; i++) {
			PyObject *item = PyList_GetItem(self->oid, i);
			oid[i] = (int)PyLong_AsLong(item);
		}
		Py_BEGIN_ALLOW_THREADS
		rv = sysctl(oid, (u_int)size, 0, 0, newval, newsize);
		Py_END_ALLOW_THREADS
		if (rv == -1) {
			switch (errno) {
			case EOPNOTSUPP:
				PyErr_SetString(
				    PyExc_TypeError, "Value is not available");
				break;
			case ENOTDIR:
				PyErr_SetString(PyExc_TypeError,
				    "Specification is incomplete");
				break;
			case ENOMEM:
				PyErr_SetString(PyExc_TypeError,
				    "Type is unknown to this program");
				break;
			default:
				PyErr_SetFromErrno(PyExc_OSError);
				break;
			}
			free(newval);
			free(oid);
			return (-1);
		}
		if (self->private.type != CTLTYPE_STRING)
			free(newval);
		free(oid);
	}

	Py_XDECREF(self->value);
	Py_INCREF(value);
	self->value = value;
	return (0);
}

static PyObject *
Sysctl_getdescr(Sysctl *self, void *closure __unused)
{
	int qoid[CTL_MAXNAME + 2];
	const int *oid;
	char *descr, *p;
	size_t len;
	u_int nlen;

	if (self->description != NULL) {
		Py_INCREF(self->description);
		return (self->description);
	}

	oid = self->private.oid;
	nlen = self->private.len;

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_OIDDESCR;
	memcpy(qoid + 2, oid, nlen * sizeof(int));

	descr = NULL;
	len = 0;
	while (sysctl(qoid, nlen + 2, descr, &len, NULL, 0) != 0 ||
	    descr == NULL) {
		if (descr != NULL && errno != 0 && errno != ENOMEM) {
			free(descr);
			return (PyErr_SetFromErrno(PyExc_OSError));
		}
		p = realloc(descr, len);
		if (p == NULL) {
			free(descr);
			return (PyErr_SetFromErrno(PyExc_OSError));
		}
		descr = p;
	}
	self->description = PyUnicode_FromString(descr);
	free(descr);

	Py_INCREF(self->description);
	return (self->description);
}

static PyGetSetDef Sysctl_getseters[] = {
	{ "value", (getter)Sysctl_getvalue, (setter)Sysctl_setvalue,
	    "sysctl value", NULL },
	{ "description", (getter)Sysctl_getdescr, (setter)NULL,
	    "sysctl description", NULL },
	{ 0 } /* Sentinel */
};

static PyMemberDef Sysctl_members[] = {
	{ "name", T_OBJECT_EX, offsetof(Sysctl, name), READONLY, "name" },
	{ "writable", T_OBJECT_EX, offsetof(Sysctl, writable), READONLY,
	    "Can be written" },
	{ "tuneable", T_OBJECT_EX, offsetof(Sysctl, tuneable), READONLY,
	    "Tuneable" },
	{ "oid", T_OBJECT_EX, offsetof(Sysctl, oid), READONLY, "OID MIB" },
	{ "type", T_UINT, offsetof(Sysctl, private.type), READONLY,
	    "Data type of sysctl" },
	{ 0 } /* Sentinel */
};

static PyTypeObject SysctlType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "Sysctl",
	.tp_basicsize = sizeof(Sysctl),
	.tp_dealloc = (destructor)Sysctl_dealloc,
	.tp_repr = (PyObject *(*)(PyObject *))Sysctl_repr,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_doc = "Sysctl objects",
	.tp_members = Sysctl_members,
	.tp_getset = Sysctl_getseters,
	.tp_init = (initproc)Sysctl_init,
};

static u_int
sysctl_type(const int *oid, u_int len, char *fmt)
{
	int qoid[CTL_MAXNAME + 2];
	u_char buf[BUFSIZ];
	size_t j;

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_OIDFMT;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof(buf);
	if (sysctl(qoid, len + 2, buf, &j, 0, 0) == -1)
		return ((u_int)-1);

	if (fmt)
		strcpy(fmt, (char *)(buf + sizeof(u_int)));

	return (*(u_int *)buf);
}

static PyObject *
new_sysctlobj(const int *oid, u_int nlen, u_int kind, const char *fmt)
{
	char name[BUFSIZ] = { '\0' };
	int qoid[CTL_MAXNAME + 2];
	size_t len, nlenb = nlen * sizeof(int);
	PyObject *selfobj, *args, *kwargs, *oidobj, *oidentry, *writable,
	    *tuneable;
	Sysctl *self;

	qoid[0] = CTL_SYSCTL;
	qoid[1] = CTL_SYSCTL_NAME;
	memcpy(qoid + 2, oid, nlenb);

	len = sizeof(name);
	if (sysctl(qoid, nlen + 2, name, &len, 0, 0) == -1)
		return (PyErr_SetFromErrno(PyExc_OSError));

	writable = PyBool_FromLong(kind & CTLFLAG_WR);
	tuneable = PyBool_FromLong(kind & CTLFLAG_TUN);

	oidobj = PyList_New(0);
	for (u_int i = 0; i < nlen; i++) {
		oidentry = PyLong_FromLong(oid[i]);
		PyList_Append(oidobj, oidentry);
		Py_DECREF(oidentry);
	}

	args = Py_BuildValue("()");
	kwargs = Py_BuildValue("{s:s,s:O,s:O,s:O}", "name", name,
	    "writable", writable, "tuneable", tuneable, "oid", oidobj);
	selfobj = PyObject_Call((PyObject *)&SysctlType, args, kwargs);
	Py_DECREF(args);
	Py_DECREF(kwargs);
	Py_DECREF(oidobj);
	Py_DECREF(writable);
	Py_DECREF(tuneable);

	self = (void *)selfobj;
	self->private.len = nlen;
	self->private.type = kind & CTLTYPE;
	self->private.oid = malloc(nlenb);
	if (self->private.oid == NULL) {
		Py_DECREF(selfobj);
		return (PyErr_SetFromErrno(PyExc_OSError));
	}
	memcpy(self->private.oid, oid, nlenb);
	self->private.fmt = strdup(fmt);
	if (self->private.fmt == NULL) {
		free(self->private.oid);
		Py_DECREF(selfobj);
		return (PyErr_SetFromErrno(PyExc_OSError));
	}

	return (selfobj);
}

static PyObject *
sysctl_filter(PyObject *self __unused, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = { "mib", "writable", NULL };
	int name1[22], name2[22], oid[CTL_MAXNAME];
	char *mib = NULL, fmt[BUFSIZ];
	PyObject *list = NULL, *writable = NULL, *new = NULL;
	size_t len = 0, l1 = 0, l2, i;
	u_int kind, ctltype;

	if (!PyArg_ParseTupleAndKeywords(
	    args, kwds, "|zO", kwlist, &mib, &writable))
		return (NULL);

	list = PyList_New(0);

	name1[0] = CTL_SYSCTL;
#ifdef CTL_SYSCTL_NEXTNOSKIP
	name1[1] = mib != NULL ? CTL_SYSCTL_NEXTNOSKIP : CTL_SYSCTL_NEXT;
#else
	name1[1] = CTL_SYSCTL_NEXT;
#endif

	if (mib != NULL && mib[0] != '\n' && strlen(mib) > 0) {
		len = nitems(oid);
		if (sysctlnametomib(mib, oid, &len) == -1)
			return (list);
		kind = sysctl_type(oid, (u_int)len, fmt);
		if (kind == (u_int)-1) {
			Py_DECREF(list);
			return (PyErr_SetFromErrno(PyExc_OSError));
		}
		ctltype = kind & CTLTYPE;
		if (ctltype == CTLTYPE_NODE) {
			memcpy(name1 + 2, oid, len * sizeof(int));
			l1 = len + 2;
		} else {
			new = new_sysctlobj(oid, (u_int)len, kind, fmt);
			PyList_Append(list, new);
			Py_DECREF(new);
			return (list);
		}
	} else {
		name1[2] = CTL_KERN;
		l1 = 3;
	}

	for (;;) {
		l2 = sizeof(name2);
		if (sysctl(name1, (u_int)l1, name2, &l2, NULL, 0) == -1) {
			if (errno == ENOENT)
				return (list);
			Py_DECREF(list);
			return (PyErr_SetFromErrno(PyExc_OSError));
		}
		l2 /= sizeof(int);
		if (l2 < len)
			return (list);
		for (i = 0; i < len; i++)
			if (name2[i] != oid[i])
				return (list);

		kind = sysctl_type(name2, (u_int)l2, fmt);
		if (kind == (u_int)-1) {
			Py_DECREF(list);
			return (PyErr_SetFromErrno(PyExc_OSError));
		}
		if (writable == Py_True && (kind & CTLFLAG_WR) == 0)
			goto next;
		if (writable == Py_False && (kind & CTLFLAG_WR) != 0)
			goto next;

		new = new_sysctlobj(name2, (u_int)l2, kind, fmt);
		PyList_Append(list, new);
		Py_DECREF(new);
next:
		memcpy(name1 + 2, name2, l2 * sizeof(int));
		l1 = l2 + 2;
	}
	Py_UNREACHABLE();
}

/* Cast through void (*) (void) to suppress -Wcast-function-type */
#define	PyCFunction_Cast(f)	((PyCFunction)(void (*) (void))(f))

static PyMethodDef SysctlMethods[] = {
	{ "filter", PyCFunction_Cast(sysctl_filter),
	    METH_VARARGS | METH_KEYWORDS, "Sysctl all" },
	{ "error_out", PyCFunction_Cast(error_out), METH_NOARGS, NULL },
	{ 0 } /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3

static int
sysctl_traverse(PyObject *m, visitproc visit, void *arg)
{
	Py_VISIT(GETSTATE(m)->error);
	return (0);
}

static int
sysctl_clear(PyObject *m)
{
	Py_CLEAR(GETSTATE(m)->error);
	return (0);
}

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	.m_name = "_sysctl",
	.m_size = sizeof(struct module_state),
	.m_methods = SysctlMethods,
	.m_traverse = sysctl_traverse,
	.m_clear = sysctl_clear,
	0,
};

#define INITERROR return (NULL)

PyMODINIT_FUNC
PyInit__sysctl(void)
#else

#define INITERROR return

PyMODINIT_FUNC
init_sysctl(void)
#endif /* PY_MAJOR_VERSION >= 3 */
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
	return (m);
#endif
}
