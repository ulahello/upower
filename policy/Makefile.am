polkit_policydir = $(datadir)/PolicyKit/policy
polkit_policy_DATA =					\
	org.freedesktop.devicekit.power.policy		\
	org.freedesktop.devicekit.power.qos.policy

# You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
@INTLTOOL_POLICY_RULE@

check:
	polkit-policy-file-validate $(top_builddir)/policy/org.freedesktop.devicekit.power.policy
	polkit-policy-file-validate $(top_builddir)/policy/org.freedesktop.devicekit.power.qos.policy

EXTRA_DIST =						\
	org.freedesktop.devicekit.power.policy.in	\
	org.freedesktop.devicekit.power.qos.policy.in

DISTCLEANFILES =					\
	org.freedesktop.devicekit.power.policy		\
	org.freedesktop.devicekit.power.qos.policy

clean-local :
	rm -f *~

