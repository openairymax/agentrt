{{/*
AgentRT Helm Chart — 通用模板助手
*/}}

{{- define "agentrt.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "agentrt.fullname" -}}
{{- if .Values.fullnameOverride }}
{{- .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- $name := include "agentrt.name" . }}
{{- if contains $name .Release.Name }}
{{- .Release.Name | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" }}
{{- end }}
{{- end }}
{{- end }}

{{- define "agentrt.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{- define "agentrt.labels" -}}
helm.sh/chart: {{ include "agentrt.chart" . }}
{{ include "agentrt.selectorLabels" . }}
{{- if .Chart.AppVersion }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
app.kubernetes.io/part-of: agentrt
{{- end }}

{{- define "agentrt.selectorLabels" -}}
app.kubernetes.io/name: agentrt
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{- define "agentrt.serviceAccountName" -}}
{{- if .Values.global.serviceAccount.create }}
{{- default (include "agentrt.fullname" .) .Values.global.serviceAccount.name }}
{{- else }}
{{- default "default" .Values.global.serviceAccount.name }}
{{- end }}
{{- end }}

{{- define "agentrt.image" -}}
{{- $registry := .image.registry | default .Values.global.image.registry }}
{{- $repo := .image.repository }}
{{- $tag := .image.tag | default .Chart.AppVersion }}
{{- if $registry }}
{{- printf "%s/%s:%s" $registry $repo $tag }}
{{- else }}
{{- printf "%s:%s" $repo $tag }}
{{- end }}
{{- end }}

{{- define "agentrt.daemon.container" -}}
- name: {{ .name | default "daemon" }}
  image: {{ include "agentrt.image" . }}
  imagePullPolicy: {{ .image.pullPolicy | default .Values.global.image.pullPolicy }}
  command: ["/usr/local/bin/{{ .name }}"]
  ports:
    - name: http
      containerPort: {{ .service.port }}
      protocol: TCP
    {{- if .service.metricsPort }}
    - name: metrics
      containerPort: {{ .service.metricsPort }}
      protocol: TCP
    {{- end }}
  env:
    - name: AGENTOS_SERVICE_NAME
      value: {{ .name | quote }}
    - name: AGENTOS_SERVICE_PORT
      value: {{ .service.port | quote }}
    - name: AGENTOS_LOG_LEVEL
      value: {{ .Values.agentosConfig.logLevel | quote }}
    - name: AGENTOS_DEPLOY_ENV
      value: {{ .Values.agentosConfig.deployEnv | quote }}
    {{- if .env }}
    {{- range $k, $v := .env }}
    - name: {{ $k }}
      value: {{ $v | quote }}
    {{- end }}
    {{- end }}
  envFrom:
    - configMapRef:
        name: {{ include "agentrt.fullname" $ }}-config
  {{- if .resources }}
  resources:
    {{- toYaml .resources | nindent 4 }}
  {{- end }}
  livenessProbe:
    httpGet:
      path: /healthz
      port: http
    initialDelaySeconds: {{ .livenessProbe.initialDelaySeconds | default 15 }}
    periodSeconds: {{ .livenessProbe.periodSeconds | default 10 }}
  readinessProbe:
    httpGet:
      path: /healthz
      port: http
    initialDelaySeconds: {{ .readinessProbe.initialDelaySeconds | default 10 }}
    periodSeconds: {{ .readinessProbe.periodSeconds | default 5 }}
  {{- if .securityContext }}
  securityContext:
    {{- toYaml .securityContext | nindent 4 }}
  {{- end }}
{{- end }}

{{- define "agentrt.daemon.service" -}}
apiVersion: v1
kind: Service
metadata:
  name: {{ include "agentrt.fullname" $ }}-{{ .name }}
  labels:
    {{- include "agentrt.labels" $ | nindent 4 }}
    app.kubernetes.io/component: {{ .name }}
spec:
  type: {{ .service.type | default "ClusterIP" }}
  ports:
    - port: {{ .service.port }}
      targetPort: http
      protocol: TCP
      name: http
    {{- if .service.metricsPort }}
    - port: {{ .service.metricsPort }}
      targetPort: metrics
      protocol: TCP
      name: metrics
    {{- end }}
  selector:
    {{- include "agentrt.selectorLabels" $ | nindent 4 }}
    app.kubernetes.io/component: {{ .name }}
{{- end }}

{{- define "agentrt.daemon.deployment" -}}
apiVersion: apps/v1
kind: Deployment
metadata:
  name: {{ include "agentrt.fullname" $ }}-{{ .name }}
  labels:
    {{- include "agentrt.labels" $ | nindent 4 }}
    app.kubernetes.io/component: {{ .name }}
spec:
  replicas: {{ .replicaCount | default $.Values.global.replicaCount }}
  selector:
    matchLabels:
      {{- include "agentrt.selectorLabels" $ | nindent 6 }}
      app.kubernetes.io/component: {{ .name }}
  template:
    metadata:
      labels:
        {{- include "agentrt.selectorLabels" $ | nindent 8 }}
        app.kubernetes.io/component: {{ .name }}
      annotations:
        {{- with $.Values.podAnnotations }}
        {{- toYaml . | nindent 8 }}
        {{- end }}
    spec:
      serviceAccountName: {{ include "agentrt.serviceAccountName" $ }}
      {{- with $.Values.global.image.pullSecrets }}
      imagePullSecrets:
        {{- toYaml . | nindent 8 }}
      {{- end }}
      containers:
        {{- include "agentrt.daemon.container" . | nindent 8 }}
      {{- with $.Values.nodeSelector }}
      nodeSelector:
        {{- toYaml . | nindent 8 }}
      {{- end }}
      {{- with $.Values.tolerations }}
      tolerations:
        {{- toYaml . | nindent 8 }}
      {{- end }}
      {{- with $.Values.affinity }}
      affinity:
        {{- toYaml . | nindent 8 }}
      {{- end }}
{{- end }}